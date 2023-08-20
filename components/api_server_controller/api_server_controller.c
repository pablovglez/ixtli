#include <lwip/sockets.h>
#include "driver/gpio.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "API-SERVER";

bool led_bool = 0;

esp_err_t send_web_page(httpd_req_t *req)
{
    //Image Settings Variables
    int jpg_quality = 10;
    int frame_size = 8;
    int length = 0;
    
    // Send HTML header
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html>");
    httpd_resp_sendstr_chunk(req, "<html><head>");
    httpd_resp_sendstr_chunk(req, "<h1>Ixtli Video Socket Server</h1>");
    httpd_resp_sendstr_chunk(req, "</head><br><hr>");

    // Send Settings
    httpd_resp_sendstr_chunk(req, "<center><h2>Settings</h2></center>");
    httpd_resp_sendstr_chunk(req, "<tr>{\"settings\":{\"jpg_quality\": ");
    length = snprintf( NULL, 0, "%d", jpg_quality );
    char* str_jpg_quality = malloc( length + 1 );
    snprintf( str_jpg_quality, length + 1, "%d", jpg_quality );
    httpd_resp_sendstr_chunk(req, str_jpg_quality);
    httpd_resp_sendstr_chunk(req, ", \"submit_img_set\": ");
    length = snprintf( NULL, 0, "%d", frame_size );
    char* str_frame_size = malloc( length + 1 );
    snprintf( str_frame_size, length + 1, "%d", frame_size );
    httpd_resp_sendstr_chunk(req, str_frame_size);
    httpd_resp_sendstr_chunk(req, "}}</tr>");
    httpd_resp_sendstr_chunk(req, "<br>");

    httpd_resp_sendstr_chunk(req, "</html>");
    /* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);
    
    free(str_jpg_quality);
    free(str_frame_size);
    return ESP_OK;
}

esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

static esp_err_t flash_led_brightness_handler(httpd_req_t *req)
{
    led_bool = (led_bool + 1 ) % 2 ;
    gpio_set_level(4, led_bool);
    ESP_LOGI(TAG, "Led is %d\n", led_bool);

    return send_web_page(req);
}

httpd_uri_t uri_home = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_flash = {
    .uri = "/flash",
    .method = HTTP_GET,
    .handler = flash_led_brightness_handler,
    .user_ctx = NULL
};

void setup_api_server(void)
{
    httpd_config_t api_config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t api_httpd  = NULL;

    if (httpd_start(&api_httpd , &api_config) == ESP_OK)
    {
        httpd_register_uri_handler(api_httpd, &uri_home);
        httpd_register_uri_handler(api_httpd, &uri_flash);
    }

    ESP_LOGI(TAG, "API Server is up and running\n");
    //return api_httpd;
}
