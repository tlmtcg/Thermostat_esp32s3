idf_component_register(
    SRCS "task_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        weather 
        weather_store 
        weather_utils 
        time_utils 
        jeedom 
        cjson 
        led_ctrl 
        app_context
        oled_service
        sd_card
        serial_manager 
        wifi 
        config_storage
        sht31
)
