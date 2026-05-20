file(GLOB U8G2_SOURCES
    "csrc/*.c"
    "csrc/*.cpp"
    "u8g2_esp32_hal.c"
)

idf_component_register(
    SRCS ${U8G2_SOURCES}
    INCLUDE_DIRS  "." "csrc"
    REQUIRES esp_driver_i2c
)
