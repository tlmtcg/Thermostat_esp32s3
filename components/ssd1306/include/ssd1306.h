#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT   64

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t address;

    uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

    bool initialized;

} ssd1306_t;

/**
 * @brief Initialise l'écran SSD1306
 */
esp_err_t ssd1306_init(
    ssd1306_t *lcd,
    i2c_master_bus_handle_t bus,
    uint8_t address
);

/**
 * @brief Efface le framebuffer
 */
void ssd1306_clear(ssd1306_t *lcd);

/**
 * @brief Envoie le framebuffer à l'écran
 */
esp_err_t ssd1306_update(ssd1306_t *lcd);

/**
 * @brief Dessine un pixel
 */
void ssd1306_draw_pixel(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    bool color
);

/**
 * @brief Affiche une chaîne simple
 */
void ssd1306_draw_string(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *str
);

#ifdef __cplusplus
}
#endif
