idf_component_register(SRCS "sd_card.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs sdmmc esp_common
    PRIV_REQUIRES esp_driver_spi esp_driver_gpio
    )