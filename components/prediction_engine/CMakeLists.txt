idf_component_register(
    SRCS "prediction_engine.c" "saison_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES cjson thermostat thermal_engine heating_program time_utils sd_card
)
