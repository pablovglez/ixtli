#include <esp_system.h>
#include <nvs_flash.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "esp_log.h"
//#include "mbedtls/aes.h"

#include "connect_wifi.h"
#include "ixtli_config.h"
#include "parameters.h"
//#include "cam_controller.h"
#include "api_server_controller.h"
//#include "mic_controller.h"
#include "stream_controller.h"


#include <stdbool.h>

#include <inttypes.h>

static char* TAG = "Ixtli esp32-cam Websocket server";

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

    // Load parameters
    loadPersistentSettings(IXTLI_CONF_FILEPATH);

    connect_wifi(global_params.project_name, global_params.wifi_ssid, global_params.wifi_pass);
    char ip_address[16] = {0};
    get_ip_address(ip_address, 16);

    if (!wifi_connect_status)
    {
        ESP_LOGI(global_params.project_name, "Failed to connected with Wi-Fi, check your network Credentials\n");
        // Wait 60 seconds before restarting
        int wait = 60;
        while (wait > 0) {
            if (wait % 10 == 0) {
                ESP_LOGI(global_params.project_name, "Restarting in %d seconds...", wait);
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            wait--;
        }
        ESP_LOGI(global_params.project_name, "Restarting now...");
        // Restart the ESP32
        esp_restart();
        return;
    }
    setup_api_server(global_params.authkey);
    // If IXTLI_PHOTOBOOTH is not defined, start the camera server
    #ifdef IXTLI_PHOTOBOOTH
        ESP_LOGI(global_params.project_name, "Ixtli in photobooth mode");
    #elif BOARD_XIAO_ESP32S3
        ESP_LOGI(global_params.project_name, "Ixtli in video mode");
    #endif

    esp_err_t err = setup_stream_server(global_params.cam_frame_size, global_params.cam_jpeg_quality);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to setup stream server: %s", esp_err_to_name(err));
        return;
    }

    setup_audio_server();

    ESP_LOGI(TAG, "Camera Ready! Use 'http://%s' to connect", ip_address);
    
    // Play greeting LED animation to indicate the server is ready
    greeting_led();

    /***** Mic Test *****/

    ESP_LOGI(TAG, "PDM microphone recording example start");
    // Mount the SDCard for recording the audio file
    //mount_sdcard();
    
    // Acquire a I2S PDM channel for the PDM digital microphone
    //init_microphone();
    ESP_LOGI(TAG, "Starting recording ");
    
    // Start Recording
    //record_wav(20); // Record for 20 seconds

    

    //xTaskCreatePinnedToCore(mic_sb_get, "mic_ringbuf_read",
    //                        4096, NULL, 5, NULL, 1);

    /***** Mic Test *****/

}