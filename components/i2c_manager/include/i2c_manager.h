#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#include "cJSON.h"

/* -------------------------------------------------------------------------- */
/*  STRUCTURE SCAN RESULT                                                     */
/* -------------------------------------------------------------------------- */

typedef struct {

    uint8_t *addresses;

    size_t count;

    char timestamp[32];

} i2c_scan_result_t;

/* -------------------------------------------------------------------------- */
/*  BUS GLOBAL                                                                */
/* -------------------------------------------------------------------------- */

extern i2c_master_bus_handle_t i2c_bus;

/* -------------------------------------------------------------------------- */
/*  API                                                                       */
/* -------------------------------------------------------------------------- */

/* create I2C bus */
esp_err_t i2c_manager_init(int sda, int scl, int freq);

/* delete I2C bus */
esp_err_t i2c_manager_deinit(void);

/* probe device */
bool i2c_device_exists(i2c_master_bus_handle_t bus,
                       uint8_t addr);

/* scan full bus */
esp_err_t i2c_manager_scan(void);

/* JSON getter */
cJSON *i2c_manager_get_devices_json(void);

/* raw scan getter */
const i2c_scan_result_t *i2c_manager_get_scan_result(void);
