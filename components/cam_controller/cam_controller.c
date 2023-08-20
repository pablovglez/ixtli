#include <lwip/sockets.h>
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"

#include "camera_pins.h"

static const char *TAG = "CAM-CONTROLLER";

#define CONFIG_XCLK_FREQ 20000000
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";


static esp_err_t init_camera(int cam_frame_size, int cam_jpeg_quality)
{
    camera_config_t camera_config = {
        .pin_pwdn  = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sscb_sda = CAM_PIN_SIOD,
        .pin_sscb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = CONFIG_XCLK_FREQ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,

        .frame_size = cam_frame_size,

        .jpeg_quality = cam_jpeg_quality
    };
    
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        return err;
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
            ESP_LOGI(TAG, "SOCKET: %u ", tx_len);
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
        ESP_LOGI(TAG, "MJPG: %uKB %ums (%.1ffps)",
        (uint32_t)(_jpg_buf_len/1024),
        (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);

        
    }
    last_frame = 0;
}


void socket_server_task(void *pvParameters) {
    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(16385);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int err = bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket server started. Waiting for client connections...");

    while (1) {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        
        setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPIDLE, &err, sizeof(int));

        char addr_str[INET_ADDRSTRLEN];
        inet_ntoa_r(client_addr.sin_addr, addr_str, sizeof(addr_str));
        ESP_LOGI(TAG, "Client connected from %s:%d", addr_str, ntohs(client_addr.sin_port));
        
        do_transmit(client_sock);

        if (listen_sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket...");
            shutdown(client_sock, 0);
            close(client_sock);
        }
    }

    vTaskDelete(NULL);
}

//void setup_stream_server(IxtliPersistentSettings global_params)
void setup_stream_server(int cam_frame_size, int cam_jpeg_quality)
{
    esp_err_t err;
    err = init_camera(cam_frame_size, cam_jpeg_quality);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "err: %s\n", esp_err_to_name(err));
            return;
        }
    xTaskCreate(socket_server_task, "socket_server_task", configMINIMAL_STACK_SIZE * 5, NULL, 5, NULL);
}