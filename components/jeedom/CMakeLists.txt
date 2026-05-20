idf_component_register(
    SRCS
        "jeedom.c"
        "jeedom_task.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        cjson
        esp_http_client
        esp_http_server
        esp_timer
        esp_wifi
        freertos
        time_utils
        weather
)
                    
