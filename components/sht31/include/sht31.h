#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct
{
    uint8_t addr;
    uint32_t read_interval_ms;
    bool log_to_sd;
} sht31_config_t;

typedef struct
{
    float temperature;
    float humidity;
    bool valid;
    time_t last_update;
    bool initialized;
    bool running;
    uint32_t read_count;
    uint32_t error_count;
    char last_error[64];
} sht31_runtime_t;

/* Alias temporaire pour compatibilite avec le code existant. */
typedef sht31_runtime_t sht31_state_t;

esp_err_t sht31_get_config(sht31_config_t *out);

esp_err_t sht31_set_config(const sht31_config_t *config);

const sht31_runtime_t *sht31_get_runtime(void);

void sht31_set_running(bool running);

char *sht31_get_json_status(void);

esp_err_t sht31_init(i2c_master_bus_handle_t bus, uint8_t addr);

void sht31_deinit(void);

esp_err_t sht31_read(float *temp, float *hum);

esp_err_t sht31_start(i2c_master_bus_handle_t bus, uint8_t addr);

void sht31_stop(void);

esp_err_t sht31_reset(void);

const sht31_state_t *sht31_get_state(void);
