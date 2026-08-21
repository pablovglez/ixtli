//
// Created by efisio on 8/2/26.
//
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <lwip/sockets.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "camera_pins.h"
#include "camera_controller.h"

#define CONFIG_XCLK_FREQ 20000000
#define LED_LEDC_CHANNEL 2

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

static const char *TAG = "CAM-CONTROLLER";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t init_camera(int cam_frame_size, int cam_jpeg_quality)
{
    camera_config_t camera_config = {
        .pin_pwdn  = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sscb_sda = SIOD_GPIO_NUM,
        .pin_sscb_scl = SIOC_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,
        .xclk_freq_hz = CONFIG_XCLK_FREQ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = (framesize_t)cam_frame_size,
        .jpeg_quality = cam_jpeg_quality,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST
      };

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        return err;
    }
    ESP_LOGI(TAG, "Camera init successfully");
    return ESP_OK;
}


esp_err_t setup_camera(int cam_frame_size, int cam_jpeg_quality){
    if (init_camera(cam_frame_size, cam_jpeg_quality) != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void do_transmit(const int sock){

    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];
    static int64_t last_frame = 0;
    int tx_len = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }


    while (1) {

        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
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
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            tx_len = send(sock, (const char *)part_buf, hlen, 0);
            tx_len  = send(sock, (const char *)_jpg_buf, _jpg_buf_len, 0);
#ifdef DEBUG_ON
            ESP_LOGI(TAG, "SOCKET: %u ", tx_len);
#endif
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(tx_len == -1){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
#ifdef DEBUG_ON
        ESP_LOGI(TAG, "MJPG: %luKB %lums (%.1ffps)",
        (uint32_t)(_jpg_buf_len/1024),
        (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
#endif


    }
    last_frame = 0;
}

static int print_reg(char * p, sensor_t * s, uint16_t reg, uint32_t mask){
    return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}


void gen_status_json(char *status){
    sensor_t *s = esp_camera_sensor_get();

    char *p = status;
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

}

esp_err_t process_cmd(char *variable, int val){
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

    if (res < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t take_snapshot(camera_fb_t *fb){
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
        //httpd_resp_send_500(req);
        return ESP_FAIL;
    }

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

    return res;
}

void return_frame_buffer(camera_fb_t* snapshot) {
    esp_camera_fb_return(snapshot);
}