#pragma once

#include "driver/i2c_master.h"
#include <stdbool.h>
#include <time.h>

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    uint8_t addr;

    bool initialized;
} sht31_t;

typedef struct {
    float temperature;
    float humidity;
    bool valid;
    time_t last_update;
} sht31_state_t;

/* API publique */
void sht31_start(i2c_master_bus_handle_t bus, uint8_t addr);

esp_err_t sht31_read(float *temp, float *hum);

const sht31_state_t *sht31_get_state(void);
