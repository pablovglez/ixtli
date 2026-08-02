idf_component_register(SRCS "camera_controller.c"
        PRIV_REQUIRES esp_timer driver esp32-camera esp_http_server
        INCLUDE_DIRS ".")