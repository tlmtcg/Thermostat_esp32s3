idf_component_register(
    SRCS
        "led_ctrl.c"
        "led_driver.c"
        "led_task.c"
        "led_db.c"

    INCLUDE_DIRS
        "include"

    REQUIRES
        esp_timer
        esp_system
        driver
        freertos
        esp_partition
        led_strip
        alert_manager
        tasks
)
