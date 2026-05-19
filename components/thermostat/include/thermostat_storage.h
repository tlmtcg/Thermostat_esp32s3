#pragma once

#include "esp_err.h"
#include "thermostat.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t thermostat_storage_load(thermostat_config_t *config);

esp_err_t thermostat_storage_save(const thermostat_config_t *config);

#ifdef __cplusplus
}
#endif
