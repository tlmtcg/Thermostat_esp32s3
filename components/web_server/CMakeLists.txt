idf_component_register(
    SRCS 
        "web_server.c"
        "ws_static.c"
        "wifi/ws_api_wifi.c"
        "wifi/wifi_service.c"
        "ws_api_time.c"
        "ws_api_weather.c"
        "led/ws_api_led.c"   
        "led/led_service.c"  
        "ws_api_alarms.c" 
        "ws_api_freebox.c" 
        "ws_api_logs.c" 
        "ws_api_sys.c" 
        "ws_api_program.c"
        "ws_api_jeedom.c"
        "ws_api_relay.c"
        "ws_api_sd.c"
        "ws_api_task.c"
        "ws_api_i2c.c"
        "ws_api_sht31.c"

    INCLUDE_DIRS 
        "include"
        "wifi/include"
        "led/include"

    # Visible depuis les headers
    REQUIRES 
        esp_http_server
        wifi
        time_utils
        led_ctrl
        weather
        weather_store
        freebox_ftp
        heating_program
        jeedom
        relay
        sd_card
        task_manager
        i2c_manager
        sht31

    # Visible uniquement dans les .c
    PRIV_REQUIRES 
        esp_wifi
        esp_netif
        esp_timer
        cjson

    EMBED_FILES 
        "www/index.html"
        "www/time.html"
        "www/wifi.html"
        "www/weather.html"
        "www/freebox.html"
        "www/alarms.html"
        "www/led.html" 
        "www/sys.html"
        "www/program.html"
        "www/relay.html"
        "www/sd.html"
        "www/nav.html"
        "www/tasks.html"
        "www/i2c.html"
        "www/logs.html"
        "www/sht31.html"
        "www/jeedom.html"
        "www/common.css"
        "www/index.js"

)