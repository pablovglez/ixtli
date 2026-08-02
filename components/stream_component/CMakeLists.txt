idf_component_register(SRCS "stream_controller.c"
    PRIV_REQUIRES esp32-camera camera_component mic_component
    INCLUDE_DIRS "."
)
