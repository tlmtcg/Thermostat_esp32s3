idf_component_register(
    SRCS "time_utils.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_netif lwip alert_manager
)