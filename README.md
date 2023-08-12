# ixtli
ESP CAM Socket Server

This is a fork of [esp32tutorials' Web server](https://github.com/ESP32Tutorials/ESP32-CAM-ESP-IDF-Live-Streaming-Web-Server) but adds an API for setting up the 
ESP32 (WiFi credentials, Flash, etc...) and streaming is also possible using websockets.

### Requirements

Before building this project download [Espressif's camera library](https://github.com/espressif/esp32-camera) and copy it in this project (under lib folder) or you can simply create a symlink to the library.

You will also need VSCode and PlatformIO