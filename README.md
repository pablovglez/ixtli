# ixtli
ESP CAM Socket Server

This is a fork of [esp32tutorials' Web server](https://github.com/ESP32Tutorials/ESP32-CAM-ESP-IDF-Live-Streaming-Web-Server) but adds an API for setting up the 
ESP32 (WiFi credentials, Flash, etc...) and streaming is also possible using websockets.

### Requirements

Before building this project download [Espressif's camera library](https://github.com/espressif/esp32-camera) and copy it in this project (under lib folder) or you can simply create a symlink to the library.

You will also need VSCode and PlatformIO

### Points to check when switching from Arduino Framework to ESP IDF

1. Updated esp-camera by cloning master version (15-08-2025)
2. Esp-camera now requires esp-jpeg as component 
3. Do not add those libraries in the platformio.ini file
    ```
    lib_deps =
        ; esp32-camera ; <-- comment or remove this line
        ; esp_jpeg   ; <-- comment or remove this line
4. Enable PSRAM using menuconfig
    - Component config --> ESP PSRAM --> Support for external, SPI-connected RAM
    - Component config --> ESP PSRAM --> SPI RAM config --> Mode (QUAD/OCT) --> Octal Mode PSRAM