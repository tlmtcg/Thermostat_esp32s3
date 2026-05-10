cmake_minimum_required(VERSION 3.22)

# Obligatoire : charge la version IDF
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# Ajout des composants externes AVANT project()
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components"
)

# Nom du projet Unity
project(Thermostat)
