idf_component_register(
    SRCS
        "thermostat.c"
        "thermostat_storage.c"
        "thermostat_manager.c"
        "thermostat_learning.c" 

    INCLUDE_DIRS
        "include"

    REQUIRES
        nvs_flash
        heating_program
        relay
        cjson
        alert_manager
        app_context
        thermal_model
        predict_adjustment
        rc_estimator
)
