#pragma once

#include <stdbool.h>

typedef enum
{
    THERMOSTAT_MODE_AUTO = 0,
    THERMOSTAT_MODE_MANUAL,
    THERMOSTAT_MODE_ABSENT,
    THERMOSTAT_MODE_HORS_GEL
} thermostat_mode_t;

typedef struct
{
    bool enabled;
    float consigne;
    thermostat_mode_t mode;
    float hysteresis;
    float calibration;
    bool frost_mode;

} thermostat_state_t;

typedef struct
{
    float current_consigne;
    float temp_ext;
    float current_temperature;
} thermostat_runtime_t;

void thermostat_init(void);

thermostat_state_t thermostat_get_state(void);

void thermostat_set_mode(thermostat_mode_t mode);

void thermostat_set_consigne(float value);

void thermostat_set_enabled(bool enabled);

const thermostat_runtime_t *thermostat_get_runtime(void);

thermostat_runtime_t *thermostat_get_runtime_rw(void);