// components/led/include/led_storage.h
#pragma once
#include "led_types.h"
#include "esp_err.h"

#define LED_CONFIG_FILE  MOUNT_POINT "/led_config.json"

esp_err_t led_storage_init(void);
esp_err_t led_storage_save(const char *data, size_t size);
esp_err_t led_storage_load(char **data, size_t *size);