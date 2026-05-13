idf_component_register(
    SRCS "sht31_task.c" "sht31.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c freertos oled_service
)
