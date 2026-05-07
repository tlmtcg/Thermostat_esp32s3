cmake_minimum_required(VERSION 3.22)

# 1. On définit le chemin de l'IDF si nécessaire
if(NOT DEFINED ENV{IDF_PATH})
    set(ENV{IDF_PATH} "C:/esp/v6.0/esp-idf")
endif()

# 2. IMPORTANT : On déclare les dossiers de composants AVANT l'include
# On ajoute le dossier "components" entier pour que tous tes sous-dossiers (wifi, led, etc.) soient vus
set(EXTRA_COMPONENT_DIRS "components")

# 3. On inclut la logique de build d'Espressif (qui va scanner tes Kconfig)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 4. On nomme le projet
project(Thermostat_esp32s3)