idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_http_server joltwallet__littlefs
    REQUIRES wifi web_server time_utils led_ctrl alert_manager task_manager serial_manager freebox_ftp
)