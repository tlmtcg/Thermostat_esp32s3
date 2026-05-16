#pragma once

#include <stdint.h>
#include "u8g2.h"

#define U8G2_ESP32_HAL_UNDEFINED (-1)

typedef struct {
    int sda;
    int scl;
    int reset;
} u8g2_esp32_hal_t;

void u8g2_esp32_hal_init(u8g2_esp32_hal_t hal);

uint8_t u8x8_gpio_and_delay(
    u8x8_t *u8x8,
    uint8_t msg,
    uint8_t arg_int,
    void *arg_ptr
);
