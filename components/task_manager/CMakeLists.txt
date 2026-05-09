idf_component_register(
    SRCS "task_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES weather weather_store weather_utils time_utils jeedom cjson
)