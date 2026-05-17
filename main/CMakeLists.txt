# Liste des fichiers sources pour l'application principale
set(my_srcs "main_app.c")

# Liste des dépendances requises
set(my_requires
    wifi
    web_server
    time_utils
    led_ctrl
    alert_manager
    task_manager
    serial_manager
    freebox_ftp
    heating_program
    relay
    sd_card
    email_service
    i2c_manager
    config_storage
    sht31
    ssd1306
    oled_service
    app_context
    display_pages
)

# Enregistrement du composant principal
idf_component_register(
    SRCS "${my_srcs}"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_http_server joltwallet__littlefs
    REQUIRES "${my_requires}"
)
