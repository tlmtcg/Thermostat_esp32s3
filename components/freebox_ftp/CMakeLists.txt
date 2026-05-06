# Enregistrement du composant pour ESP-IDF
idf_component_register(
    SRCS "freebox_ftp.c"
    INCLUDE_DIRS "include"           
    REQUIRES esp_http_client mbedtls log 
)
