#include <lwip/sockets.h>
#include "driver/gpio.h"

#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_https_ota.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "lwip/apps/sntp.h"
#include <esp_ota_ops.h>
#include <time.h>
#include <sys/time.h>
#include "esp_camera.h"
#include "camera_controller.h"

// Max value for LEDC duty cycle (8-bit resolution)

#define CONFIG_LED_MAX_INTENSITY 255  // 2^8 - 1 = 255
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER   LEDC_TIMER_0

static const char *TAG = "API-SERVER";

char auth_token[56];

static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;
bool led_bool = false;

// Function to handle OTA updates
static esp_err_t ota_post_handler(httpd_req_t *req) {
    esp_err_t err;

    // Get the OTA partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_ERR_INVALID_STATE;
    }

    // Start OTA
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Begin OTA");

    // Write data to OTA partition
    char ota_buf[1024];
    int data_read;
    while ((data_read = httpd_req_recv(req, ota_buf, sizeof(ota_buf))) > 0) {
        err = esp_ota_write(update_handle, ota_buf, data_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed %s", esp_err_to_name(err));
            esp_ota_end(update_handle);
            return err;
        }
    }

    // End OTA
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed %s", esp_err_to_name(err));
        return err;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA update successful, restarting...");

    // Send HTTP response
    const char *resp_str = "OTA update successful, restarting...\n";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, resp_str);
    // Wait for the response to be sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

esp_err_t auth_middleware(httpd_req_t *req) {
    esp_err_t err = ESP_OK;
    char buf[56];  // Adjust the buffer size based on your requirements

    size_t buf_size = sizeof(buf);
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authkey");
    if(!auth_len) {
        ESP_LOGE(TAG, "Missing authentication header!");
        err = ESP_ERR_HTTPD_INVALID_REQ;
        return ESP_FAIL;
    }
    else {
        httpd_req_get_hdr_value_str(req, "Authkey", buf, buf_size);
        if (strcmp(buf, auth_token) == 0) {
            // Authentication successful
            return ESP_OK;
        }
        return ESP_FAIL;
    }
    return err;
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = 0;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            *obuf = buf;
            return ESP_OK;
        }
        free(buf);
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t cmd_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char variable[32];
    char value[32];

    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK ||
        httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK) {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    free(buf);

    int val = atoi(value);
    ESP_LOGI(TAG, "%s = %d", variable, val);
    // Pass the command to the camera controller
    err_t res = process_cmd(variable, val);
    

    if (res < 0) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t snapshot_handler(httpd_req_t *req){
    esp_err_t res = ESP_OK;

    camera_fb_t* snapshot = NULL;
    res  = take_snapshot(snapshot);    

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if(res == ESP_OK){
        char ts[32];
        snprintf(ts, 32, "%lld.%06ld", snapshot->timestamp.tv_sec, snapshot->timestamp.tv_usec);
        res = httpd_resp_send(req, (const char *)snapshot->buf, snapshot->len);
    } else {
        httpd_resp_send_500(req);
    }
    return_frame_buffer(snapshot);

    return res;
}

static esp_err_t snapshot_protected(httpd_req_t *req){
    if(auth_middleware(req) != ESP_OK){
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Failed to authenticate");
        return ESP_FAIL;
    }
    return snapshot_handler(req);
}

static esp_err_t reboot_handler(httpd_req_t *req){
    /*if(auth_middleware(req) != ESP_OK){
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Failed to authenticate");
        return ESP_FAIL;
    }*/
    ESP_LOGI(TAG, "Received reboot request. Rebooting in 5 seconds...");
    httpd_resp_send(req, "Rebooting...", strlen("Rebooting..."));
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    static char json_response[1024];

    gen_status_json(json_response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

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
    char* str_jpg_quality = (char*)malloc( length + 1 );
    snprintf( str_jpg_quality, length + 1, "%d", jpg_quality );
    httpd_resp_sendstr_chunk(req, str_jpg_quality);
    httpd_resp_sendstr_chunk(req, ", \"submit_img_set\": ");
    length = snprintf( NULL, 0, "%d", frame_size );
    char* str_frame_size =  (char*)malloc( length + 1 );
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

void set_led_intensity(uint32_t duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void greeting_led() {
    const int DOT = 100 / portTICK_PERIOD_MS;
    const int DASH = 300 / portTICK_PERIOD_MS;
    const int SYMBOL_PAUSE = 100 / portTICK_PERIOD_MS;
    const int LETTER_PAUSE = 300 / portTICK_PERIOD_MS;
    const uint32_t INTENSITY = (uint32_t) CONFIG_LED_MAX_INTENSITY * 0.1;

    set_led_intensity(0); // Start with LED off
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Morse for IXTLI: .. -..- - .-.. .. -.-.--
    // I: ..
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(LETTER_PAUSE);

    // X: -..-
    set_led_intensity(INTENSITY); vTaskDelay(DASH);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DASH);
    set_led_intensity(0); vTaskDelay(LETTER_PAUSE);

    // T: -
    set_led_intensity(INTENSITY); vTaskDelay(DASH);
    set_led_intensity(0); vTaskDelay(LETTER_PAUSE);

    // L: .-..
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DASH);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(LETTER_PAUSE);

    // I: ..
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(SYMBOL_PAUSE);
    set_led_intensity(INTENSITY); vTaskDelay(DOT);
    set_led_intensity(0); vTaskDelay(LETTER_PAUSE);
    
    set_led_intensity(0); // Ensure LED is off at the end
}

void setup_led_flash()
{   
    ESP_LOGI(TAG, "Setting up flash");
}

static esp_err_t enable_led(httpd_req_t *req)
{ // Turn LED On or Off
#if FLASH_PIN > 0
    led_bool = (led_bool + 1 ) % 2 ;
    #ifdef DEBUG_ON
    ESP_LOGI(TAG, "LED flash is now %s", led_bool ? "ON" : "OFF");
    #endif
    set_led_intensity(led_bool ? CONFIG_LED_MAX_INTENSITY : 0);
#else
    ESP_LOGI(TAG, "LED flash is not available");
#endif

    return send_web_page(req);
}

esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

httpd_uri_t uri_home = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
    };

httpd_uri_t uri_flash = {
    .uri = "/flash",
    .method = HTTP_GET,
    .handler = enable_led,
    .user_ctx = NULL
};

httpd_uri_t cmd_uri = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = cmd_handler,
        .user_ctx = NULL
};

httpd_uri_t snapshot_uri = {
        .uri = "/snapshot/protected",
        .method = HTTP_GET,
        .handler = snapshot_protected,
        .user_ctx = NULL
};

httpd_uri_t ota_uri = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = ota_post_handler,
    .user_ctx = NULL
};

httpd_uri_t reboot_uri = {
    .uri = "/reboot",
    .method = HTTP_POST,
    .handler = reboot_handler,
    .user_ctx = NULL
};

void initialize_sntp() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "europe.pool.ntp.org");
    sntp_init();
}

void print_current_time() {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

void obtain_time() {
    time_t now;
    struct tm timeinfo = { 0 };

    // Wait for time to be set
    const int retry_count = 10;
    int retry = 0;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for NTP time...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void setup_api_server(char * given_key)
{   
    setup_led_flash();
    httpd_config_t api_config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t api_httpd  = NULL;
    strcpy(auth_token, given_key);

    if (httpd_start(&api_httpd , &api_config) == ESP_OK)
    {
        httpd_register_uri_handler(api_httpd, &uri_home);
        httpd_register_uri_handler(api_httpd, &uri_flash);
        httpd_register_uri_handler(api_httpd, &cmd_uri);
        httpd_register_uri_handler(api_httpd, &snapshot_uri);
        httpd_register_uri_handler(api_httpd, &reboot_uri);
    }

    ESP_LOGI(TAG, "API Server is up and running\n");
    
}

