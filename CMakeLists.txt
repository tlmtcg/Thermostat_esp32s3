# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# Force la détection si l'environnement VS Code échoue
if(NOT DEFINED ENV{IDF_PATH})
    set(ENV{IDF_PATH} "C:/esp/v6.0/esp-idf")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Thermostat_esp32s3)
list(APPEND EXTRA_COMPONENT_DIRS components/led)