#ifndef __STREAM_CONTROLLER_H__
#define __STREAM_CONTROLLER_H__

#include "esp_err.h"

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
 * @brief Initiates audio server
 * @return Error code
 * This function initializes the audio server to transmit audio frames.
 */
esp_err_t setup_audio_server(void);


#endif