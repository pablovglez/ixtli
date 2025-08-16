#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define UUID_SZ         37
#define CONF_LINE_SIZE  64
#define MAX_ADV_NAME    20

typedef enum {
    PROJECT_NAME,
    WF_SSID,
    WF_PASS,
    CAM_JPEG_QUALITY,
    CAM_FRAME_SIZE,
    AUTHKEY,
    PARAM_END
} IxtliParamEnum;

typedef struct Settings {
  char project_name[20];
  char wifi_ssid[UUID_SZ];
  char wifi_pass[UUID_SZ];
  int cam_jpeg_quality;
  int cam_frame_size;
  char authkey[302];
} IxtliPersistentSettings;

extern IxtliPersistentSettings global_params;

/**
 * @brief Read persistent parameters from a file
 * 
 * @return Number of parameters successfully read
 */
int loadPersistentSettings(const char* filename);

/**
 * @brief Write persistent parameters to a file
 * 
 * @return Number of parameters successfully written
 */
int pushPersistentSettings(const char* filename);

/**
 * @brief Read the settings file, and print each line to the console
 * 
 * @return Number of parameters successfully read
 */
int printPersistentSettingsFile(const char* filename);

/**
 * @brief Reset the default configuration (to the default_params variable value) and write it to the file
 * 
 * @return Number of parameters successfully written
 */
int resetPersistentSettings(const char* filename);

#endif