#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>

/* =========================
 *  GLOBAL BUS
 * ========================= */
extern i2c_master_bus_handle_t i2c_bus;

/* =========================
 *  SCAN STRUCT
 * ========================= */
typedef struct {
    uint8_t *addresses;
    size_t count;
    char timestamp[32];
} i2c_scan_result_t;

/* =========================
 *  API
 * ========================= */
esp_err_t i2c_manager_init(void);
esp_err_t i2c_manager_scan(void);

bool i2c_device_exists(i2c_master_bus_handle_t bus, uint8_t addr);

cJSON *i2c_manager_get_devices_json(void);
const i2c_scan_result_t *i2c_manager_get_scan_result(void);

extern i2c_master_bus_handle_t i2c_bus;