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
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1  //software reset will be performed
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y7_GPIO_NUM 35
#define Y6_GPIO_NUM 34
#define Y5_GPIO_NUM 39
#define Y4_GPIO_NUM 36
#define Y3_GPIO_NUM 21
#define Y2_GPIO_NUM 19
#define Y1_GPIO_NUM 18
#define Y0_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#endif

#endif // CAMERA_PINS_H_