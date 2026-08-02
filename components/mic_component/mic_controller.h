#ifndef __MIC_CONTROLLER_H__
#define __MIC_CONTROLLER_H__

#include <stdint.h>
#include "sys/time.h"

/**
 * @brief Data structure of microphone sample buffer
 */
typedef struct {
    uint8_t * data;              /*!< Pointer to the audio data */
    size_t len;                 /*!< Length of the buffer in bytes */
    struct timeval timestamp;   /*!< Timestamp since boot of the first DMA buffer of the frame */
} mic_sb_t;

/*
 * @brief Initialize the microphone
 */
void setup_microphone(void);

/*
 * @brief Transmit audio data over a socket
 * @param sock The socket to transmit data over
 */
void do_transmit_audio(const int sock);

#endif