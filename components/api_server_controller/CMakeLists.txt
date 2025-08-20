idf_component_register(SRCS "api_server_controller.c"
    REQUIRES driver 
    INCLUDE_DIRS "."
    REQUIRES cam_controller esp32-camera 
    PRIV_REQUIRES esp_https_server esp_partition esp_https_ota app_update
    )
