#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "tlacuilo_logger.h"

static const char *TAG = "TLACUILO";

static const esp_partition_t *s_log_partition = NULL;
static size_t s_write_offset = 0;
static bool s_logging_active = false;
static nvs_handle_t s_nvs_handle;

// Queue variables
static QueueHandle_t s_log_queue = NULL;
static TaskHandle_t s_log_task_handle = NULL;
static uint32_t s_dropped_messages = 0;
static uint32_t s_total_messages = 0;

#define LOG_QUEUE_LENGTH 30
#define LOG_QUEUE_ITEM_SIZE 256
#define LOG_PARTITION_SUBTYPE 0x99
#define NVS_NAMESPACE "log_mgr"
#define NVS_KEY_OFFSET "write_ptr"

// Structure for log messages
typedef struct {
    char message[256];
    uint32_t timestamp;
} log_queue_item_t;

// Function to save the current write pointer to NVS
static esp_err_t save_write_offset(void) {
    esp_err_t err = nvs_set_u32(s_nvs_handle, NVS_KEY_OFFSET, s_write_offset);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save write offset to NVS: %s", esp_err_to_name(err));
        return err;
    }
    return nvs_commit(s_nvs_handle);
}

// The dedicated task that writes to flash
static void log_writer_task(void *pvParameters) {
    log_queue_item_t item;
    char write_buffer[1024]; // Buffer for batch writes
    size_t buffer_offset = 0;
    
    while (1) {
        // Wait for log messages
        if (xQueueReceive(s_log_queue, &item, portMAX_DELAY) == pdTRUE) {
            size_t message_len = strlen(item.message);
            
            // Check if message fits in buffer, else flush
            if (buffer_offset + message_len > sizeof(write_buffer)) {
                if (s_write_offset + buffer_offset > s_log_partition->size) {
                    s_write_offset = 0; // Wrap around
                }
                
                esp_partition_write(s_log_partition, s_write_offset, write_buffer, buffer_offset);
                s_write_offset += buffer_offset;
                buffer_offset = 0;
                save_write_offset();
            }
            
            // Add to buffer
            memcpy(write_buffer + buffer_offset, item.message, message_len);
            buffer_offset += message_len;
            
            // If queue is empty, flush buffer to avoid delay
            if (uxQueueMessagesWaiting(s_log_queue) == 0 && buffer_offset > 0) {
                if (s_write_offset + buffer_offset > s_log_partition->size) {
                    s_write_offset = 0;
                }
                
                esp_partition_write(s_log_partition, s_write_offset, write_buffer, buffer_offset);
                s_write_offset += buffer_offset;
                buffer_offset = 0;
                save_write_offset();
            }
        }
    }
}

// Function to load the write pointer from NVS
static esp_err_t load_write_offset(void) {
    esp_err_t err = nvs_get_u32(s_nvs_handle, NVS_KEY_OFFSET, &s_write_offset);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First boot, initialize to 0
        s_write_offset = 0;
        ESP_LOGI(TAG, "No previous write offset found, starting at 0");
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read write offset from NVS: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Loaded write offset from NVS: %d bytes", s_write_offset);
    return ESP_OK;
}

void tlacuilo_log(const char *tag, const char *format, ...) {
    if (!s_logging_active || !s_log_queue) {
        return; // Logger not initialized
    }
    
    va_list args;
    va_start(args, format);
    
    log_queue_item_t item;
    item.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Format the message with timestamp and tag
    int prefix_len = snprintf(item.message, sizeof(item.message), 
                             "[T+%lu][%s] ", item.timestamp, tag);
    
    if (prefix_len > 0) {
        vsnprintf(item.message + prefix_len, sizeof(item.message) - prefix_len, format, args);
    }
    
    va_end(args);
    
    // Try to send to queue with short timeout
    BaseType_t queue_status = xQueueSend(s_log_queue, &item, pdMS_TO_TICKS(5));
    
    s_total_messages++;
    if (queue_status != pdPASS) {
        s_dropped_messages++;
        // Optional: occasional warning
        if (s_dropped_messages % 50 == 0) {
            ESP_LOGW("CUSTOM_LOGGER", "Queue full! Dropped %ld messages", s_dropped_messages);
        }
    }
}
/*
int custom_log_vprintf(const char *format, va_list args) {
    // 1. First, let the original function handle it
    int result = s_original_log_func(format, args);
    
    // 2. Only proceed if we're active and have a queue
    if (!s_logging_active || !s_log_queue) {
        return result;
    }
    
    // 3. Format the message
    log_queue_item_t item;
    item.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    vsnprintf(item.message, sizeof(item.message), format, args);
    
    // 4. Send to queue (non-blocking!)
    xQueueSend(s_log_queue, &item, 0); // 0 = don't block if queue is full
    
    return result;
}
*/
void tlacuilo_logger_start(void) {
    if (s_logging_active) {
        return;
    }

    s_log_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 
                                              LOG_PARTITION_SUBTYPE, "log_data");
    if (!s_log_partition) {
        ESP_LOGE(TAG, "Log partition not found!");
        nvs_close(s_nvs_handle);
        return;
    }
    
    // Load offset
    if (load_write_offset() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load write offset");
        nvs_close(s_nvs_handle);
        return;
    }
    
    // Create queue
    s_log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_queue_item_t));
    if (!s_log_queue) {
        ESP_LOGE(TAG, "Failed to create queue!");
        nvs_close(s_nvs_handle);
        return;
    }
    
    // Create writer task
    xTaskCreate(log_writer_task, "log_writer", 4096, NULL, 1, &s_log_task_handle);
    
    s_logging_active = true;
    ESP_LOGI(TAG, "Custom logger started. Partition: %ld bytes, Offset: %d", 
            s_log_partition->size, s_write_offset);
}

void tlacuilo_logger_stop(void) {
    if (!s_logging_active) return;
    
    // Flush any remaining messages
    vTaskDelay(pdMS_TO_TICKS(100)); // Give time to process queue
    
    if (s_log_task_handle) {
        vTaskDelete(s_log_task_handle);
        s_log_task_handle = NULL;
    }
    
    if (s_log_queue) {
        vQueueDelete(s_log_queue);
        s_log_queue = NULL;
    }
    
    save_write_offset();
    nvs_close(s_nvs_handle);
    s_logging_active = false;
    ESP_LOGI(TAG, "Custom logger stopped");
}


// Enhanced dump function that handles wrap-around
void tlacuilo_logger_dump_to_serial(void) {
    if (!s_log_partition) {
        printf("Logger not initialized!\n");
        return;
    }
    
    printf("\n--- Custom Log Dump ---\n");
    char *buffer = malloc(s_log_partition->size);
    if (!buffer) {
        printf("Memory allocation failed!\n");
        return;
    }
    
    esp_partition_read(s_log_partition, 0, buffer, s_log_partition->size);
    
    // Print until we hit erased memory
    for (size_t i = 0; i < s_log_partition->size; i++) {
        if (buffer[i] == 0xFF) break;
        putchar(buffer[i]);
    }
    
    free(buffer);
    printf("\n--- End of Dump ---\n");
}

void tlacuilo_logger_erase(void) {
    if (s_log_partition) {
        esp_partition_erase_range(s_log_partition, 0, s_log_partition->size);
        s_write_offset = 0;
        save_write_offset();
        ESP_LOGI(TAG, "Log partition erased and pointer reset");
    }
}

void custom_logger_get_stats(uint32_t *total, uint32_t *dropped) {
    if (total) *total = s_total_messages;
    if (dropped) *dropped = s_dropped_messages;
}