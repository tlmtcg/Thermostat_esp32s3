idf_component_register(
    SRCS "prediction_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES cjson thermostat rc_estimator heating_program time_utils
)
