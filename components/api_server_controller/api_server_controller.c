#include <lwip/sockets.h>
#include "driver/gpio.h"

#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "lwip/apps/sntp.h"
#include <esp_ota_ops.h>
#include <time.h>
#include <sys/time.h>
#include "esp_camera.h"

#include "cam_controller.h"

#define CONFIG_LED_MAX_INTENSITY 255

static const char *TAG = "API-SERVER";

char auth_token[56];

static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;
bool led_bool = 0;

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
    /*
    sensor_t *s = esp_camera_sensor_get();

    int res = 0;

    if (!strcmp(variable, "framesize")) {
        if (s->pixformat == PIXFORMAT_JPEG) {
            res = s->set_framesize(s, (framesize_t)val);
        }
    }
    else if (!strcmp(variable, "quality"))
        res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast"))
        res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness"))
        res = s->set_brightness(s, val);
    else if (!strcmp(variable, "saturation"))
        res = s->set_saturation(s, val);
    else if (!strcmp(variable, "gainceiling"))
        res = s->set_gainceiling(s, (gainceiling_t)val);
    else if (!strcmp(variable, "colorbar"))
        res = s->set_colorbar(s, val);
    else if (!strcmp(variable, "awb"))
        res = s->set_whitebal(s, val);
    else if (!strcmp(variable, "agc"))
        res = s->set_gain_ctrl(s, val);
    else if (!strcmp(variable, "aec"))
        res = s->set_exposure_ctrl(s, val);
    else if (!strcmp(variable, "hmirror"))
        res = s->set_hmirror(s, val);
    else if (!strcmp(variable, "vflip"))
        res = s->set_vflip(s, val);
    else if (!strcmp(variable, "awb_gain"))
        res = s->set_awb_gain(s, val);
    else if (!strcmp(variable, "agc_gain"))
        res = s->set_agc_gain(s, val);
    else if (!strcmp(variable, "aec_value"))
        res = s->set_aec_value(s, val);
    else if (!strcmp(variable, "aec2"))
        res = s->set_aec2(s, val);
    else if (!strcmp(variable, "dcw"))
        res = s->set_dcw(s, val);
    else if (!strcmp(variable, "bpc"))
        res = s->set_bpc(s, val);
    else if (!strcmp(variable, "wpc"))
        res = s->set_wpc(s, val);
    else if (!strcmp(variable, "raw_gma"))
        res = s->set_raw_gma(s, val);
    else if (!strcmp(variable, "lenc"))
        res = s->set_lenc(s, val);
    else if (!strcmp(variable, "special_effect"))
        res = s->set_special_effect(s, val);
    else if (!strcmp(variable, "wb_mode"))
        res = s->set_wb_mode(s, val);
    else if (!strcmp(variable, "ae_level"))
        res = s->set_ae_level(s, val);
    else {
        ESP_LOGI(TAG, "Unknown command: %s", variable);
        res = -1;
    }
    */

    if (res < 0) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}



static esp_err_t snapshot_handler(httpd_req_t *req){
    esp_err_t res = ESP_OK;
    /*
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;

    int8_t retries = 5;

    while (retries > 0)
    {
        fb = esp_camera_fb_get();
        if (!fb){
            retries =- 1;
        }
        else
            break;
    }

    if (!fb)
    {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    */

    camera_fb_t* snapshot = NULL;
    res  = take_snapshot(snapshot);    

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    /*
    char ts[32];
    snprintf(ts, 32, "%ld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
    httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

    if(fb->format != PIXFORMAT_JPEG){
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        if(!jpeg_converted){
            ESP_LOGE(TAG, "JPEG compression failed");
            esp_camera_fb_return(fb);
            res = ESP_FAIL;
        }
    } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
    }
    */
    
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


static esp_err_t xclk_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char _xclk[32];

    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_query_key_value(buf, "xclk", _xclk, sizeof(_xclk)) != ESP_OK) {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    free(buf);

    int xclk = atoi(_xclk);
    ESP_LOGI(TAG, "Set XCLK: %d MHz", xclk);

    sensor_t *s = esp_camera_sensor_get();
    int res = s->set_xclk(s, LEDC_TIMER_0, xclk);
    if (res) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}


static esp_err_t status_handler(httpd_req_t *req)
{
    static char json_response[1024];

    gen_status_json(json_response);
    
    /*
    sensor_t *s = esp_camera_sensor_get();
    char *p = json_response;
    *p++ = '{';

    p+=print_reg(p, s, 0xd3, 0xFF);
    p+=print_reg(p, s, 0x111, 0xFF);
    p+=print_reg(p, s, 0x132, 0xFF);


    p += sprintf(p, "\"xclk\":%u,", s->xclk_freq_hz / 1000000);
    p += sprintf(p, "\"pixformat\":%u,", s->pixformat);
    p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p += sprintf(p, "\"quality\":%u,", s->status.quality);
    p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p += sprintf(p, "\"awb\":%u,", s->status.awb);
    p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p += sprintf(p, "\"aec\":%u,", s->status.aec);
    p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p += sprintf(p, "\"agc\":%u,", s->status.agc);
    p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);
    *p++ = '}';
    *p++ = 0;
    */
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

esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

static esp_err_t enable_led(httpd_req_t *req)
{ // Turn LED On or Off
    led_bool = (led_bool + 1 ) % 2 ;
    //ledcWrite(2, led_bool*CONFIG_LED_MAX_INTENSITY);

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
    httpd_config_t api_config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t api_httpd  = NULL;
    strcpy(auth_token, given_key);

    if (httpd_start(&api_httpd , &api_config) == ESP_OK)
    {
        httpd_register_uri_handler(api_httpd, &uri_home);
        httpd_register_uri_handler(api_httpd, &uri_flash);
        httpd_register_uri_handler(api_httpd, &cmd_uri);
        httpd_register_uri_handler(api_httpd, &snapshot_uri);
    }

    ESP_LOGI(TAG, "API Server is up and running\n");
    //return api_httpd;
}

