#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

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
} thermostat_config_t;

typedef struct
{
    bool initialized;
    float effective_consigne;
    uint32_t change_count;
    char last_error[64];
} thermostat_runtime_t;

/* Alias temporaire pour compatibilite avec le code existant. */
typedef thermostat_config_t thermostat_state_t;

void thermostat_init(void);

esp_err_t thermostat_get_config(thermostat_config_t *out);

esp_err_t thermostat_set_config(const thermostat_config_t *config);

const thermostat_runtime_t *thermostat_get_runtime(void);

char *thermostat_get_json_status(void);

thermostat_state_t thermostat_get_state(void);

void thermostat_set_mode(thermostat_mode_t mode);

void thermostat_set_consigne(float value);

void thermostat_set_enabled(bool enabled);
