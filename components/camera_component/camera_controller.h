//
// Created by efisio on 8/2/26.
//

#ifndef IXTLI_CAMERA_CONTROLLER_H
#define IXTLI_CAMERA_CONTROLLER_H

#include "esp_camera.h"

/**
 * @brief Sets up the camera
 * @param cam_frame_size Frame size for the camera
 * @param cam_jpeg_quality JPEG quality for the camera
 */
void setup_camera(int cam_frame_size, int cam_jpeg_quality);

/**
 * @brief Initiates cam and streaming server
 * @param cam_frame_size Frame size for the camera
 * @param cam_jpeg_quality JPEG quality for the camera
 * @return Error code
 * This function initializes the camera with the specified frame size and JPEG quality,
 * and starts the streaming server to transmit camera frames.
 */
void do_transmit(const int sock);

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
 * @param snapshot Pointer to the camera frame buffer
 * @return Error code
 * This function captures a snapshot from the camera and returns it in the provided frame buffer.
 * If the capture fails, it returns ESP_FAIL.
 */
esp_err_t take_snapshot(camera_fb_t *snapshot);

/**
 * @brief Returns the frame buffer
 * @param snapshot Pointer to the camera frame buffer
 * This function releases the frame buffer back to the camera driver after processing.
 * It is important to call this function after using the frame buffer to avoid memory leaks.
 */
void return_frame_buffer(camera_fb_t* snapshot);

/**
 * @brief Generates the status json
 * @param status Pointer to the status string buffer
 * This function generates a JSON string containing the current status of the camera settings.
 * It includes information such as xclk frequency, pixel format, frame size, quality, brightness
 */
void gen_status_json(char *status);

/**
 * @brief Sets up the LED flash
 *
 * This function configures the LED flash for the camera.
 */

#endif //IXTLI_CAMERA_CONTROLLER_H
