#include "u8g2_esp32_hal.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static u8g2_esp32_hal_t u8g2_hal;

void u8g2_esp32_hal_init(u8g2_esp32_hal_t hal)
{
    u8g2_hal = hal;

    if (hal.sda != U8G2_ESP32_HAL_UNDEFINED) {
        gpio_reset_pin(hal.sda);
        gpio_set_direction(hal.sda, GPIO_MODE_OUTPUT_OD);
        gpio_set_pull_mode(hal.sda, GPIO_PULLUP_ONLY);
    }

    if (hal.scl != U8G2_ESP32_HAL_UNDEFINED) {
        gpio_reset_pin(hal.scl);
        gpio_set_direction(hal.scl, GPIO_MODE_OUTPUT_OD);
        gpio_set_pull_mode(hal.scl, GPIO_PULLUP_ONLY);
    }

    if (hal.reset != U8G2_ESP32_HAL_UNDEFINED) {
        gpio_reset_pin(hal.reset);
        gpio_set_direction(hal.reset, GPIO_MODE_OUTPUT);
    }
}

uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8,
                            uint8_t msg,
                            uint8_t arg_int,
                            void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;

    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10);
        break;

    case U8X8_MSG_DELAY_100NANO:
        break;

    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;

    case U8X8_MSG_GPIO_RESET:
        if (u8g2_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
            gpio_set_level(u8g2_hal.reset, arg_int);
        break;

    default:
        break;
    }

    return 1;
}
