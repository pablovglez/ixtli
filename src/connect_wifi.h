#ifndef CONNECT_WIFI_H_
#define CONNECT_WIFI_H_

#include <esp_system.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

void get_mac_address(uint8_t *mac);

void get_ip_address(char *ip_address, size_t size);

void connect_wifi(char* project_name, char* wifi_ssid, char * wifi_password);

bool is_wifi_connected();

#endif