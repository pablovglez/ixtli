#include "connect_wifi.h"
#include "esp_netif.h"
#include "lwip/apps/mdns.h"
#include "esp_mac.h"
#include <stdbool.h>

bool wifi_connected = false;
static const char *TAG = "Connect_WiFi";
int s_retry_num = 0;

#define MAXIMUM_RETRY 6
/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

void reboot_on_wifi_disconnection(int timeout_sec) {
    ESP_LOGI(TAG, "Failed to connected with Wi-Fi, check your network Credentials\n");
    // Wait 60 seconds before restarting
    int wait = timeout_sec;
    while (wait > 0) {
        if (wait % 10 == 0) {
            wifi_connected = is_wifi_connected();
            if (wifi_connected) {
                break;
            }
            ESP_LOGI(TAG, "Restarting in %d seconds...", wait);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        wait--;
    }
    if (wait < 10) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi, check your network connection");

        ESP_LOGI(TAG, "Restarting now...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        // Restart the ESP32
        esp_restart();
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        wifi_connected = false;
        ESP_LOGI(TAG, "connect to the AP fail");
        reboot_on_wifi_disconnection(60);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connected = true;
    }
}

void get_ip_address(char *ip_address, size_t size)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        snprintf(ip_address, size, IPSTR, IP2STR(&ip_info.ip));
    }
    else
    {
        snprintf(ip_address, size, "0.0.0.0");
    }
}

void get_mac_address(uint8_t *mac)
{
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

void connect_wifi(char* project_name, char* wifi_ssid, char * wifi_password)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    
    // Get ESP32 MAC address
    uint8_t mac[6];
    get_mac_address(mac);

    // Set hostname with last 3 bytes of MAC address
    char hostname[32];
    //snprintf(hostname, sizeof(hostname), "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5]);
    snprintf(hostname, sizeof(hostname), "%s-%02X%02X%02X", project_name, mac[3], mac[4], mac[5]);
    
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, hostname));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    if(wifi_ssid != NULL || wifi_password != NULL){
        memcpy(wifi_config.sta.ssid, wifi_ssid, strlen(wifi_ssid));
        memcpy(wifi_config.sta.password, wifi_password, strlen(wifi_password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 wifi_config.sta.ssid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 wifi_config.sta.ssid);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    vEventGroupDelete(s_wifi_event_group);
}

bool is_wifi_connected()
{
    return wifi_connected;
}
