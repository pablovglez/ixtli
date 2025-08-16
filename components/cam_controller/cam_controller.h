#ifndef __CAM_CONTROLLER_H__
#define __CAM_CONTROLLER_H__

#include "esp_camera.h"

/**
 * @brief Initiates cam and streaming server
 * @param cam_frame_size Frame size for the camera
 * @param cam_jpeg_quality JPEG quality for the camera
 * @return Error code
 * This function initializes the camera with the specified frame size and JPEG quality,
 * and starts the streaming server to transmit camera frames.
 */
esp_err_t setup_stream_server(int cam_frame_size, int cam_jpeg_quality);

/**
 * @brief Processes the command
 * @param variable The command variable
 * @param val The value to set for the command
 * @return Error code
 * This function processes the command by setting the specified variable to the given value.
 * It handles various camera settings such as framesize, quality, contrast, brightness, etc.
 * If the command is unknown, it logs an error and returns ESP_FAIL.
 */
esp_err_t process_cmd(char *variable, int val);

/**
 * @brief Takes a snapshot
 * 
 * @return Error code
 */
esp_err_t take_snapshot(camera_fb_t *snapshot);

/**
 * @brief Returns the frame buffer
 * 
 */
void return_frame_buffer(void* snapshot);

/**
 * @brief Generates the status json
 * 
 * @param status 
 */
void gen_status_json(char *status);

#endif