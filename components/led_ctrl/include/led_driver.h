// components/led/include/led_driver.h
#pragma once
#include "led_types.h"
#include "esp_err.h"

esp_err_t led_driver_init(void);
void led_driver_set_pixel(led_color_t color);
void led_driver_clear(void);
void led_driver_refresh(void);
void led_driver_set_brightness(uint8_t brightness);