cmake_minimum_required(VERSION 3.22)

# Obligatoire : charge la version IDF
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# Ajout des composants externes AVANT project()
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components"
)

# Obligatoire : charge le système de build IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nom du projet Unity
project(example_test_app)
