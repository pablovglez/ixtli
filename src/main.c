#include <esp_system.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <inttypes.h>
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#if defined(WIFI_MODE)
#include "connect_wifi.h"
#elif defined(ETHERNET_MODE)
#include "ethernet_controller.h"
#endif
#include "ixtli_config.h"
#include "parameters.h"
#include "api_server_controller.h"
#include "stream_controller.h"



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

#if defined(WIFI_MODE)
    connect_wifi(global_params.project_name, global_params.wifi_ssid, global_params.wifi_pass);
    bool wifi_connected = is_wifi_connected();
    if (!wifi_connected)
    {
        reboot_on_wifi_disconnection(60);
    }

    char ip_address[16] = {0};
    get_ip_address(ip_address, 16);
#elif defined(ETHERNET_MODE)
    ret = ethernet_init();
    bool eth_connected = is_eth_connected();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ethernet library: %s", esp_err_to_name(ret));
    }
    if (!eth_connected)
    {
        reboot_on_eth_disconnection(60);
    }
    char ip_address[16] = {0};
    get_ip_address_eth(ip_address, 16);
#endif
    setup_api_server(global_params.authkey);
    esp_err_t err = setup_stream_server(global_params.cam_frame_size, global_params.cam_jpeg_quality);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to setup stream server: %s", esp_err_to_name(err));
        return;
    }
#if defined(STREAM_AUDIO)
    err = setup_audio_server();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to setup stream server: %s", esp_err_to_name(err));
        return;
    }
#endif
    ESP_LOGI(TAG, "Camera Ready! Use 'http://%s' to connect", ip_address);

    // Play greeting LED animation to indicate the server is ready
#if LEDC_OUTPUT_IO > 0
    greeting_led();
#endif

}