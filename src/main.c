#include <esp_system.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include <inttypes.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "driver/gpio.h"
#include "parameters.h"
#include "connect_wifi.h"
#include "ixtli_config.h"
#include "api_server_controller.h"
#include "stream_controller.h"
#include "tlacuilo_logger.h"



// Define our custom partition subtype from the CSV table
#define LOG_PARTITION_SUBTYPE 0x99

static char* TAG = "IXTLI";

/**** Logging partition ****/

/*
// Simple structure for our log entries
typedef struct {
    uint32_t timestamp; // Simple timestamp (e.g., from millis())
    char message[64];   // The log message itself
} log_entry_t;

*/

/**** Logging partition ****/

static esp_err_t init_spiffs(void){
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 20,   // This decides the maximum number of files that can be created on the storage
      .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Mount/Format failed");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }
    else {
        size_t spiffs_total = 0, spiffs_used = 0;
        esp_spiffs_info(NULL, &spiffs_total, &spiffs_used);
        ESP_LOGI(TAG, "SPIFFS space used %dB/%dB.", spiffs_used, spiffs_total);
    }
    return ESP_OK;
}


void app_main(){
    ESP_ERROR_CHECK(init_spiffs());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    /**** Logging partition ****/

    /*
    // 1. FIND the custom partition
    const esp_partition_t *log_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, // Type: data
        LOG_PARTITION_SUBTYPE,   // Subtype: our custom 0x99
        "log_data"               // Name: must match partitions.csv
    );
    if (log_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find log partition!");
        return;
    }
    ESP_LOGI(TAG, "Found log partition at 0x%lux, size: 0x%lux bytes", log_partition->address, log_partition->size);

    // 2. WRITE a new log entry (append to the end)
    log_entry_t new_log;
    new_log.timestamp = esp_log_timestamp(); // Get a timestamp
    strcpy(new_log.message, "Hello World from Custom Partition!");

    // Find the next write address. We'll just append after the last entry.
    // This is a simple example and doesn't handle wrap-around.
    static size_t next_write_addr = 0;
    // For a real application, you would need to manage this offset in NVS or find the last valid entry.

    // For this demo, let's just write to the beginning if it's the first time.
    if (next_write_addr == 0) {
        next_write_addr = 0;
    }

    printf("Writing log at offset 0x%x...\n", next_write_addr);
    ret = esp_partition_write(log_partition, next_write_addr, &new_log, sizeof(new_log));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Write successful!");
        next_write_addr += sizeof(new_log); // Move pointer for next write
    }
    */

    tlacuilo_logger_start();

    /**** Logging partition ****/

    // Load parameters
    loadPersistentSettings(IXTLI_CONF_FILEPATH);

    connect_wifi(global_params.project_name, global_params.wifi_ssid, global_params.wifi_pass);
    char ip_address[16] = {0};
    get_ip_address(ip_address, 16);

    if (!wifi_connect_status)
    {
        ESP_LOGI(global_params.project_name, "Failed to connected with Wi-Fi, check your network Credentials\n");
        tlacuilo_log(global_params.project_name, "Failed to connected with Wi-Fi, check your network Credentials\n");
        // Wait 60 seconds before restarting
        int wait = 60;
        while (wait > 0) {
            if (wait % 10 == 0) {
                ESP_LOGI(global_params.project_name, "Restarting in %d seconds...", wait);
                tlacuilo_log(global_params.project_name, "Restarting in %d seconds...", wait);
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            wait--;
        }
        ESP_LOGI(global_params.project_name, "Restarting now...");
        tlacuilo_log(global_params.project_name, "Restarting now...");
        // Restart the ESP32
        esp_restart();
        return;
    }
    setup_api_server(global_params.authkey);
    // If IXTLI_PHOTOBOOTH is not defined, start the camera server
    #ifdef IXTLI_PHOTOBOOTH
        ESP_LOGI(global_params.project_name, "Ixtli in photobooth mode");
        tlacuilo_log(global_params.project_name, "Ixtli in photobooth mode");
    #elif BOARD_XIAO_ESP32S3
        ESP_LOGI(global_params.project_name, "Ixtli in video mode");
        tlacuilo_log(global_params.project_name, "Ixtli in video mode");
    #endif

    esp_err_t err = setup_stream_server(global_params.cam_frame_size, global_params.cam_jpeg_quality);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to setup stream server: %s", esp_err_to_name(err));
        tlacuilo_log(global_params.project_name, "Failed to setup stream server: %s", esp_err_to_name(err));
        return;
    }

    #ifndef IXTLI_PHOTOBOOTH
    // In photobooth mode, we do not use the audio server
    setup_audio_server();
    #endif

    ESP_LOGI(TAG, "Camera Ready! Use 'http://%s' to connect", ip_address);
    tlacuilo_log(global_params.project_name, "Camera Ready! Use 'http://%s' to connect", ip_address);
    
    // Play greeting LED animation to indicate the server is ready
    greeting_led();

}