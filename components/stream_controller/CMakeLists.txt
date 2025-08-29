idf_component_register(SRCS "stream_controller.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp32-camera cam_controller mic_controller)
