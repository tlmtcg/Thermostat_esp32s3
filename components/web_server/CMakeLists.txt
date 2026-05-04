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
        "www/led.html" 
        "www/sys.html"
        "www/nav.html"
        "www/logs.html"
        "www/common.css"
        "www/index.js"

)