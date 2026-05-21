idf_component_register(
    SRCS
        "sht31.c"
        "sht31_task.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        app_context
        cjson
        esp_driver_i2c 
        freertos 
        i2c_manager 
        sd_card
        time_utils
        alert_manager
        thermostat
)
