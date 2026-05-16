file(GLOB U8G2_SOURCES
    "u8g2/csrc/*.c"
    "u8g2/csrc/*.cpp"
    "u8g2_esp32_hal.c"
)

idf_component_register(
    SRCS ${U8G2_SOURCES}
    INCLUDE_DIRS  "." "u8g2/csrc"
    REQUIRES esp_driver_i2c
)
