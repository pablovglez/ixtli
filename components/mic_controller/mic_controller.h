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

void mount_sdcard(void);
void setup_microphone(void);
void record_wav(uint32_t rec_time);

void mic_ringbuf_fill(void *vParameters); // Remove from header
size_t mic_ringbuf_read(int16_t *dest, size_t max_samples); // Remove from header
void mic_sb_get(void); 
void do_transmit_audio(const int sock);

#endif