#pragma once

#include "esp_err.h"
#include "thermostat.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t thermostat_storage_load(thermostat_state_t *state);

esp_err_t thermostat_storage_save(const thermostat_state_t *state);

#ifdef __cplusplus
}
#endif