idf_component_register(
    SRCS "ssd1306_font.c" "ssd1306.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
)