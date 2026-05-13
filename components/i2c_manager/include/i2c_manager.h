#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Déclaration du bus I2C et du mutex en externe
extern i2c_master_bus_handle_t i2c_bus;
extern SemaphoreHandle_t i2c_mutex;

// Macro pour verrouiller/déverrouiller le mutex
#define I2C_LOCK()    xSemaphoreTake(i2c_mutex, portMAX_DELAY)
#define I2C_UNLOCK()  xSemaphoreGive(i2c_mutex)

typedef struct {
    uint8_t *addresses;
    size_t count;
    char timestamp[64];
} i2c_scan_result_t;

esp_err_t i2c_manager_init(int sda, int scl, int freq);
esp_err_t i2c_manager_deinit(void);
bool i2c_device_exists(i2c_master_bus_handle_t bus, uint8_t addr);
esp_err_t i2c_manager_scan(void);
cJSON *i2c_manager_get_devices_json(void);
const i2c_scan_result_t *i2c_manager_get_scan_result(void);
