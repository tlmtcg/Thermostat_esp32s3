idf_component_register(
    SRCS  "ssd1306.c" "font3x5.c" "font5x7.c" "font8x16.c" "font16x24.c" 
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c i2c_manager oled_service sht31
)