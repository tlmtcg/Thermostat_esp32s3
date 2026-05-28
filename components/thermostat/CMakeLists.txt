idf_component_register(
    SRCS
        "thermostat.c"
        "thermostat_storage.c"
        "thermostat_manager.c"

    INCLUDE_DIRS
        "include"

    REQUIRES
        nvs_flash
        heating_program
        relay
        cjson
        alert_manager
        app_context
        thermal_engine
        prediction_engine
)
