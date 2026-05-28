idf_component_register(
    SRCS "prediction_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES cjson thermostat thermal_engine heating_program time_utils
)
