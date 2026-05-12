idf_component_register(
    SRCS "weather.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_netif cjson esp-tls alert_manager
    # Inclure le certificat dans le binaire
    EMBED_FILES "certs/open_meteo_cert.pem"
)
