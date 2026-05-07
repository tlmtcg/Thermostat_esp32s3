idf_component_register(
    SRCS "serial_manager.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES alert_manager esp_driver_uart freertos wifi esp_wifi heating_program
)

