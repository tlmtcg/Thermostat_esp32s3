# Enregistrement du composant pour ESP-IDF
idf_component_register(
    SRCS 
         SRCS "freebox_ftp.c"
    INCLUDE_DIRS "include"           # Dossier contenant vos headers (.h)
    REQUIRES lwip log                # Dépendances internes (TCP/IP et Logs)
)
