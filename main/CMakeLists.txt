idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_http_server spiffs
    REQUIRES wifi web_server time_utils led_ctrl 
)