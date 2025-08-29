#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2s_pdm.h"
#include "driver/spi_master.h"
#include <lwip/sockets.h>
#include "mic_controller.h"


// I2S settings
#define CONFIG_MIC_I2S_CLK 42
#define CONFIG_MIC_I2S_DATA 41

// Audio settings do not change for best
#define SAMPLE_RATE 16000
#define VOLUME_GAIN 16.0f
#define I2S_READ_CHUNK    1024        // Number of bytes to read per call

// Ring buffer to hold microphone samples
#define RINGBUF_SIZE      (16 * 1024) // 16KB buffer
#define RINGBUF_SAMPLES (RINGBUF_SIZE / sizeof(int16_t))

static int16_t ringbuf[RINGBUF_SAMPLES];
static size_t write_pos = 0;
static size_t read_pos = 0;

static i2s_chan_handle_t rx_handle = NULL;

static bool s_state = false;
static SemaphoreHandle_t mic_data_sem;

static const char *TAG = "MIC-CONTROLLER";


static esp_err_t init_microphone(void)
{
    mic_data_sem = xSemaphoreCreateBinary();
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_clk_config_t clock = {
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = SOC_MOD_CLK_PLL_F160M, // `I2S_CLK_SRC_DEFAULT` defaults to this
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .dn_sample_mode = I2S_PDM_DSR_8S,
    }; 

  i2s_pdm_rx_slot_config_t slot = {
        .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT, // "only support 16 bits for PDM mode"
        .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT, // "only support 16 bits for PDM mode"
        .slot_mode = I2S_SLOT_MODE_MONO,
        /* "The default mono slot is the left slot (whose 'select pin' of the PDM microphone is pulled down)" */
        .slot_mask = I2S_PDM_SLOT_LEFT, 
        /* "I2S PDM only transmits or receives the PDM device whose 'select' pin is pulled down" */
    };

    i2s_pdm_rx_gpio_config_t gpio = {
        .clk = CONFIG_MIC_I2S_CLK, // note that the word select is used as the clock
        .din = CONFIG_MIC_I2S_DATA,
        .invert_flags = {.clk_inv = false,},
    };

    i2s_pdm_rx_config_t rx_config = {
        .clk_cfg = clock,
        .slot_cfg = slot,
        .gpio_cfg = gpio,
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &rx_config));

    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));


    // Set the state to indicate that the microphone is initialized
    s_state = ESP_OK;

    return s_state;
}

static size_t advance_pos(size_t pos, size_t inc) {
    return (pos + inc) % RINGBUF_SAMPLES;
}

// Producer: fill ring buffer with I2S samples
void mic_ringbuf_fill(void *vParameters) {
    size_t bytes_read = 0;
    int16_t temp[I2S_READ_CHUNK / 2];

    while (1){
        if (i2s_channel_read(rx_handle, temp, I2S_READ_CHUNK, &bytes_read, 1000) == ESP_OK) {
            size_t samples_read = bytes_read / sizeof(int16_t);

            // Apply volume gain
            for (size_t i = 0; i < samples_read; i++) {
                int32_t amplified_sample = (int32_t)temp[i] * VOLUME_GAIN;
                if (amplified_sample > INT16_MAX)  amplified_sample = INT16_MAX;
                if (amplified_sample < INT16_MIN) amplified_sample = INT16_MIN;
                temp[i] = (int16_t)amplified_sample;
            }

            size_t next_write = advance_pos(write_pos, samples_read);
            if (next_write == read_pos) {
                read_pos = advance_pos(read_pos, samples_read);
                if (read_pos == write_pos) {
                    read_pos = 0;
                }

                // TODO: Add an Automatic Gain Control (AGC) mechanism here
            }
            if (write_pos + samples_read <= RINGBUF_SAMPLES) {
                memcpy(&ringbuf[write_pos], temp, samples_read * sizeof(int16_t));
            } else {
                size_t first_part = (RINGBUF_SAMPLES) - write_pos;
                memcpy(&ringbuf[write_pos], temp, first_part * sizeof(int16_t));
                memcpy(&ringbuf[0], temp + first_part, (samples_read - first_part) * sizeof(int16_t));
            }
            write_pos = next_write;
            xSemaphoreGive(mic_data_sem);
        }
    }
}

 
// Consumer: read samples from ring buffer
size_t mic_ringbuf_read(int16_t *dest, size_t max_len) {
    if (read_pos == write_pos) {
        // Buffer empty
        return 0;
    }
    size_t available;
    if (write_pos > read_pos) {
        available = write_pos - read_pos;
    } else {
        available = (RINGBUF_SAMPLES) - read_pos + write_pos;
    }
    size_t to_read = (available < max_len) ? available : max_len;

    if (read_pos + to_read <= RINGBUF_SAMPLES) {
        memcpy(dest, &ringbuf[read_pos], to_read * sizeof(int16_t));
    } else {
        size_t first_part = (RINGBUF_SAMPLES) - read_pos;
        memcpy(dest, &ringbuf[read_pos], first_part * sizeof(int16_t));
        memcpy(dest + first_part, &ringbuf[0], (to_read - first_part) * sizeof(int16_t));
    }
    read_pos = advance_pos(read_pos, to_read);
    return to_read;
}

void do_transmit_audio(const int sock){
    
        // TODO: We continue to transmit even if socket is closed,
        // need to pass the socket state to this function to stop when needed
        // Also, we can only connect once

    int16_t sample_buf[I2S_READ_CHUNK];
    static int64_t last_sample = 0;
    int tx_len = 0;

    if (!last_sample) {
        last_sample = esp_timer_get_time();
    } 

    while (1) {
        if (xSemaphoreTake(mic_data_sem, portMAX_DELAY) == pdTRUE) {
            size_t samples_read = mic_ringbuf_read(sample_buf, I2S_READ_CHUNK);

            if (samples_read > 0) {
                size_t bytes_to_send = samples_read * sizeof(int16_t);

                tx_len = send(sock, (const char *)sample_buf, bytes_to_send, 0);

#ifdef DEBUG_ON
                ESP_LOGI(TAG,
                         "Socket %d: %d bytes sent, %d samples, first 4: %04X %04X %04X %04X",
                         sock,
                         tx_len, (int)samples_read,
                         (uint16_t)sample_buf[0],
                         (uint16_t)sample_buf[1],
                         (uint16_t)sample_buf[2],
                         (uint16_t)sample_buf[3]);
#endif
            }
            
            if(tx_len == -1){
                break;
            }
            int64_t sample_end = esp_timer_get_time();
            int64_t sample_time = sample_end - last_sample;
            last_sample = sample_end;
            sample_time /= 1000;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    last_sample = 0;
}


void setup_microphone(void)
{
    if (init_microphone() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize microphone");
    }
    else {
        ESP_LOGI(TAG, "Microphone initialized successfully");
        // mic_ringbuf_fill in a task pinned to core 0
        xTaskCreatePinnedToCore(mic_ringbuf_fill, "mic_ringbuf_fill",
                                2048, NULL, 5, NULL, 0);
    }
    
}