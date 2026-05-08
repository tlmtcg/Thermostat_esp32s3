# Alert Manager (inclut alert_storage + led_backend)

idf_component_register(
    SRCS
        "alert_manager.c"
        "alert_storage.c"
        "led_backend.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        led_ctrl     # pour led_db_get_alarm_idx_by_name, led_db_simulate
        sd_card
)

