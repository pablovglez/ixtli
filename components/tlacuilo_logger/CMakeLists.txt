idf_component_register(SRCS "tlacuilo_logger.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_partition nvs_flash
    )