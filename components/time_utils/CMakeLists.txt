idf_component_register(
    SRCS "time_utils.c" "time_utils_storage.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_netif lwip alert_manager nvs_flash
)