idf_component_register(
    SRCS "prediction_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES thermal_model cjson thermostat rc_estimator
)
