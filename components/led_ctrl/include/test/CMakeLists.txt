idf_component_register(
    SRCS "test_led_simulate.c"
    INCLUDE_DIRS "."
    REQUIRES alert_manager led_db unity
)
