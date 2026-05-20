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
        app_context
        alert_manager
        time_utils
        relay
        weather_store
        weather
)
