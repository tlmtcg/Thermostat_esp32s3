#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

/* -------------------------------------------------------------------------- */
/*  STATE PUBLIC                                                              */
/* -------------------------------------------------------------------------- */

typedef struct {

    float temperature;
    float humidity;
    bool valid;
    time_t last_update;

} sht31_state_t;

/* -------------------------------------------------------------------------- */
/*  API                                                                        */
/* -------------------------------------------------------------------------- */

/* init device only */
esp_err_t sht31_init(i2c_master_bus_handle_t bus,
                     uint8_t addr);

/* remove device + stop task */
void sht31_deinit(void);

/* single read */
esp_err_t sht31_read(float *temp,
                     float *hum);

/* start background task */
esp_err_t sht31_start(i2c_master_bus_handle_t bus,
                      uint8_t addr);

/* stop background task */
void sht31_stop(void);

/* soft reset sensor */
esp_err_t sht31_reset(void);

/* get last state */
const sht31_state_t *sht31_get_state(void);
