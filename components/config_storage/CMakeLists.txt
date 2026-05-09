idf_component_register(
    SRCS "config_storage.c"
    INCLUDE_DIRS "include"
    # vfs est nécessaire pour les fonctions fopen, fwrite, stat, etc.
    # esp_log est nécessaire pour les macros ESP_LOGI, ESP_LOGE
    REQUIRES vfs esp_log sd_card
)