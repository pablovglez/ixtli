#ifndef CAMERA_PINS_H_
#define CAMERA_PINS_H_

// Freenove ESP32-WROVER CAM Board PIN Map
#if defined(BOARD_XIAO_ESP32S3)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

#elif defined(BOARD_ESP32S3_ETH_CAM)
#define PWDN_GPIO_NUM   8
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   3
#define SIOD_GPIO_NUM   48
#define SIOC_GPIO_NUM   47
#define Y9_GPIO_NUM     18
#define Y8_GPIO_NUM     15
#define Y7_GPIO_NUM     38
#define Y6_GPIO_NUM     40
#define Y5_GPIO_NUM     42
#define Y4_GPIO_NUM     46
#define Y3_GPIO_NUM     45
#define Y2_GPIO_NUM     41
#define VSYNC_GPIO_NUM  1
#define HREF_GPIO_NUM   2
#define PCLK_GPIO_NUM   39

#define LED_GPIO_NUM    -1

#else //BOARD_ESP32CAM
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

#endif // CAMERA_PINS_H_