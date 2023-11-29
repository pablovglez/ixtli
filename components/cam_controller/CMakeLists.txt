idf_component_register(SRCS "cam_controller.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_http_server esp32-camera filesystem esp_timer)
