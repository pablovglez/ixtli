#include <errno.h>
#include <lwip/sockets.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "camera_controller.h"
#include "mic_controller.h"

static const char *TAG = "STREAM-CONTROLLER";
#if !defined(STREAM_VIDEO_PORT)
    #define STREAM_VIDEO_PORT   16385
#endif
#if !defined(STREAM_AUDIO_PORT)
    #define STREAM_AUDIO_PORT   16386
#endif

void socket_server_task(void *pvParameters) {
    int addr_family = 0;
    int ip_protocol = 0;

    // Create a socket for the video stream server
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(STREAM_VIDEO_PORT);

    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Bind the video socket to the server address
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

    ESP_LOGI(TAG, "Socket server started on port %d. Waiting for client connections...", ntohs(server_addr.sin_port));

    while (1) {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            close(listen_sock);
            vTaskDelete(NULL);
            return;
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

#ifdef STREAM_AUDIO
void audio_socket_server_task(void *pvParameters) {
    int addr_family = 0;
    int ip_protocol = 0;

    // Create a socket for the audio stream server
    struct sockaddr_in audio_server_addr;
    audio_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    audio_server_addr.sin_family = AF_INET;
    audio_server_addr.sin_port = htons(STREAM_AUDIO_PORT);

    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    // Create and ind the audio socket to the server address
    int audio_listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (audio_listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create audio socket: errno %d", errno);
        // We don't return as we can still serve video
    }

    esp_err_t err = bind(audio_listen_sock, (struct sockaddr *)&audio_server_addr, sizeof(audio_server_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Audio socket unable to bind: errno %d", errno);
        close(audio_listen_sock);
        // We don't return as we can still serve video
    }

    err = listen(audio_listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during audio listen: errno %d", errno);
        close(audio_listen_sock);
        // We don't return as we can still serve video
    }

    ESP_LOGI(TAG, "Audio socket server started on port %d. Waiting for client connections...", ntohs(audio_server_addr.sin_port));

    while (1) {

        struct sockaddr_in audio_client_addr;
        socklen_t audio_client_addr_len = sizeof(audio_client_addr);

        int audio_client_sock = accept(audio_listen_sock, (struct sockaddr *)&audio_client_addr, &audio_client_addr_len);
        if (audio_client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept audio connection: errno %d", errno);
            // We do not break as video can still be stramed
        }

        setsockopt(audio_client_sock, IPPROTO_TCP, TCP_KEEPIDLE, &err, sizeof(int));

        char addr_str_audio[INET_ADDRSTRLEN];
        inet_ntoa_r(audio_client_addr.sin_addr, addr_str_audio, sizeof(addr_str_audio));
        ESP_LOGI(TAG, "Audio client connected from %s:%d", addr_str_audio, ntohs(audio_client_addr.sin_port));

        // Also handle audio transmission if the audio socket is valid
        do_transmit_audio(audio_client_sock);
                
        if (audio_client_sock != -1) {
            ESP_LOGE(TAG, "Shutting down audio socket...");
            shutdown(audio_client_sock, 0);
            close(audio_client_sock);
        }

    }

    vTaskDelete(NULL);
}

esp_err_t setup_audio_server(void)
{
    // Setup the microphone
    setup_microphone();
    xTaskCreate(audio_socket_server_task, "audio_socket_server_task", 4096 * 5, NULL, 5, NULL);

    return ESP_OK;
}
#endif

esp_err_t setup_stream_server(int cam_frame_size, int cam_jpeg_quality)
{
    // Setup the camera
    if (setup_camera(cam_frame_size, cam_jpeg_quality) != ESP_OK) {
        ESP_LOGE(TAG, "Camera setup failed");
        return ESP_FAIL;
    }

    #ifdef BOARD_XIAO_ESP32S3
        xTaskCreate(socket_server_task, "socket_server_task", 6144 * 5, NULL, 5, NULL);
    #else
        xTaskCreate(socket_server_task, "socket_server_task", configMINIMAL_STACK_SIZE * 5, NULL, 5, NULL);
    #endif

    return ESP_OK;
}