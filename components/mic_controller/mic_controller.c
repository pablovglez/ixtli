/***** Mic Test *****/
#include "esp_log.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "driver/i2s_pdm.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "format_wav.h"
#include "mic_controller.h"
#include <string.h>
#include "esp_timer.h"
#include <lwip/sockets.h>

// I2S settings
#define CONFIG_SDCARD_SPI_MOSI_GPIO 9//10
#define CONFIG_SDCARD_SPI_MISO_GPIO 8 //9 // Not used
#define CONFIG_SDCARD_SPI_SCLK_GPIO 7 //8

#define CONFIG_MIC_I2S_CLK 42
#define CONFIG_MIC_I2S_DATA 41

// Recording Settings
#define RECORD_TIME   20  // seconds, The maximum value is 240
#define WAV_FILE_NAME "ixtli_rec"

// Audio settings do not change for best
#define SAMPLE_RATE 16000
#define BYTE_RATE 16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN 2

#define RINGBUF_SIZE      (16 * 1024) // 16KB buffer
#define I2S_READ_CHUNK    1024        // Number of bytes to read per call
//#define I2S_READ_CHUNK    128        // Number of bytes to read per call

#define RINGBUF_SAMPLES (RINGBUF_SIZE / sizeof(int16_t))
static int16_t ringbuf[RINGBUF_SAMPLES];
static size_t write_pos = 0;
static size_t read_pos = 0;

static i2s_chan_handle_t rx_handle = NULL;

#define SPI_DMA_CHAN        SPI_DMA_CH_AUTO

// SD card 
#define CONFIG_EXAMPLE_SPI_CS_GPIO 21
#define SD_MOUNT_POINT      "/sdcard"

#define PIN_NUM_MISO   9
#define PIN_NUM_MOSI   10
#define PIN_NUM_CLK    8
#define PIN_NUM_CS     21

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdmmc_card_t *card;

#define SB_GET_TIMEOUT (4000 / portTICK_PERIOD_MS)
static bool s_state = false;
QueueHandle_t sample_buffer_queue;
static SemaphoreHandle_t mic_data_sem;

static const char *TAG = "MIC-CONTROLLER";
static int16_t i2s_readraw_buff[RECORD_TIME];
size_t bytes_read;
const int WAVE_HEADER_SIZE = 44;
/***** Mic Test *****/

void mount_sdcard(void)
{
    /*
    esp_err_t ret;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 8 * 1024
    };
    ESP_LOGI(TAG, "Initializing SD card");
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_SDCARD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SDCARD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SDCARD_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    host.max_freq_khz = 1000; // Lower speed for reliability
    slot_config.gpio_cs = CONFIG_EXAMPLE_SPI_CS_GPIO;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    */

    //GPT
    esp_log_level_set("*", ESP_LOG_INFO);  // default
    esp_log_level_set("sdspi_host", ESP_LOG_DEBUG);
    esp_log_level_set("spi_master", ESP_LOG_DEBUG);
    esp_log_level_set("sdmmc_cmd", ESP_LOG_DEBUG);
    esp_log_level_set("sdmmc_common", ESP_LOG_DEBUG);
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_DEBUG);

    esp_err_t ret;
    //sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;        // HSPI like Arduino
    host.max_freq_khz = 400;      // very slow init

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_SDCARD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SDCARD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SDCARD_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16 * 1024,
    };

    // Disable DMA (Arduino does this)
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("SDCARD", "Failed to init SPI bus: %s", esp_err_to_name(ret));
        return;
    }

    // Force pullups
    gpio_set_direction(CONFIG_EXAMPLE_SPI_CS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_EXAMPLE_SPI_CS_GPIO, 1);
    gpio_set_pull_mode(CONFIG_SDCARD_SPI_SCLK_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_SDCARD_SPI_MISO_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_SDCARD_SPI_MOSI_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CONFIG_EXAMPLE_SPI_CS_GPIO, GPIO_PULLUP_ONLY);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_EXAMPLE_SPI_CS_GPIO;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config,
                                  &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("SDCARD", "Mount failed: %s", esp_err_to_name(ret));
        return;
    }

    sdmmc_card_print_info(stdout, card);
}

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

static esp_err_t init_microphone2(void)
{
    mic_data_sem = xSemaphoreCreateBinary();
#if SOC_I2S_SUPPORTS_PDM2PCM
    ESP_LOGI(TAG, "Receive PDM microphone data in PCM format");
#else
    ESP_LOGI(TAG, "Receive PDM microphone data in raw PDM format");
#endif  // SOC_I2S_SUPPORTS_PDM2PCM
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        /* The default mono slot is the left slot (whose 'select pin' of the PDM microphone is pulled down) */
#if SOC_I2S_SUPPORTS_PDM2PCM
        .slot_cfg = I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
#else
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
                    
#endif
        .gpio_cfg = {
            .clk = CONFIG_MIC_I2S_CLK,
            .din = CONFIG_MIC_I2S_DATA,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
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
            size_t next_write = advance_pos(write_pos, samples_read);
            if (next_write == read_pos) {
                read_pos = advance_pos(read_pos, samples_read);
                if (read_pos == write_pos) {
                    read_pos = 0;
                }
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

void mic_sb_get(void)
{   
    int16_t sample_buf[I2S_READ_CHUNK];
    while (1) {
        
        // Wait for new data
        if (xSemaphoreTake(mic_data_sem, portMAX_DELAY) == pdTRUE) {
            size_t samples_read = mic_ringbuf_read(sample_buf, I2S_READ_CHUNK);
            for (size_t i = 0; i < samples_read && i < 8; i++) {
                ESP_LOGI(TAG, "Sample[%d]: %d", i, sample_buf[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
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
                ESP_LOGI(TAG, "Socket value is %d", sock);
                ESP_LOGI(TAG,
                         "SOCKET: %d bytes sent, %d samples, first 4: %04X %04X %04X %04X",
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

void record_wav(uint32_t rec_time)
{
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    //ESP_LOGI(TAG, "Opening file");

    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(flash_rec_time, 16, SAMPLE_RATE, 1);

    // First check if file exists before creating a new file.
    
    struct stat st;
    if (stat(SD_MOUNT_POINT"/record.wav", &st) == 0) {
        // Delete it if it exists
        unlink(SD_MOUNT_POINT"/record.wav");
    }

    // Create new WAV file
    FILE *f = fopen(SD_MOUNT_POINT"/record.wav", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    fwrite(&wav_header, sizeof(wav_header), 1, f);

    // Start recording
    while (flash_wr_size < flash_rec_time) {
        // Read the RAW samples from the microphone
        if (i2s_channel_read(rx_handle, (char *)i2s_readraw_buff, RECORD_TIME, &bytes_read, 1000) == ESP_OK) {
            printf("[0] %d [1] %d [2] %d [3]%d ...\n", i2s_readraw_buff[0], i2s_readraw_buff[1], i2s_readraw_buff[2], i2s_readraw_buff[3]);
            // Write the samples to the WAV file
            fwrite(i2s_readraw_buff, bytes_read, 1, f);
            flash_wr_size += bytes_read;
        } else {
            printf("Read Failed!\n");
        }
        
    }

    ESP_LOGI(TAG, "Recording done!");
    fclose(f);
    ESP_LOGI(TAG, "File written on SDCard");

    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "Card unmounted");
    // Deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);

    // Stop I2S driver and destroy
    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(rx_handle));

    
    /*
    // Read the RAW samples from the microphone and log them
    while (flash_wr_size < flash_rec_time) {
        if (i2s_channel_read(rx_handle, (char *)i2s_readraw_buff, RECORD_TIME, &bytes_read, 1000) == ESP_OK) {
            ESP_LOGI(TAG,"[0] %d [1] %d [2] %d [3]%d ...\n", i2s_readraw_buff[0], i2s_readraw_buff[1], i2s_readraw_buff[2], i2s_readraw_buff[3]);
        }
        else {
            ESP_LOGI(TAG,"Read Failed!\n");
        }
    }
    */
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