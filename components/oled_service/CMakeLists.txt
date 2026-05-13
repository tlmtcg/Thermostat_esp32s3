idf_component_register(
    SRCS
        "oled_service.c"

    INCLUDE_DIRS
        "include"

    REQUIRES
        ssd1306
        esp_common
        log
        sht31
)