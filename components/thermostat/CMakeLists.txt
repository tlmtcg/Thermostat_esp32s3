idf_component_register(
    SRCS
        "thermostat.c"
        "thermostat_storage.c"
        "thermostat_manager.c"

    INCLUDE_DIRS
        "include"

    REQUIRES
        alert_manager
        app_context
        cjson
        heating_program
        nvs_flash
        prediction_engine
        relay
        thermal_engine
        weather
)
