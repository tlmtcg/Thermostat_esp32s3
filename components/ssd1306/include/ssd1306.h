#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  OLED CONFIG                                                              */
/* -------------------------------------------------------------------------- */

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8)

/* -------------------------------------------------------------------------- */
/*  STRUCTURE DEVICE                                                         */
/* -------------------------------------------------------------------------- */

typedef struct {
    const uint8_t *bitmap;   // tableau brut
    uint8_t width;           // largeur d’un caractère
    uint8_t height;          // hauteur d’un caractère
    uint8_t spacing;         // espace entre caractères
} ssd1306_font_t;

typedef struct {
    i2c_master_dev_handle_t dev;   // handle I2C device
    uint8_t address;               // I2C address (0x3C / 0x3D)
    bool initialized;              // state
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
    const ssd1306_font_t *font;
} ssd1306_t;

/* -------------------------------------------------------------------------- */
/*  CORE API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * Init OLED (wrapper de reinit)
 */
esp_err_t ssd1306_init(
    ssd1306_t *lcd,
    i2c_master_bus_handle_t bus,
    uint8_t address
);

/**
 * Re-init complet (safe reattach I2C device)
 */
esp_err_t ssd1306_reinit(
    ssd1306_t *lcd,
    i2c_master_bus_handle_t bus,
    uint8_t address
);

/**
 * Remove device from I2C bus
 */
esp_err_t ssd1306_deinit(ssd1306_t *lcd);

/* -------------------------------------------------------------------------- */
/*  DRAW PRIMITIVES                                                          */
/* -------------------------------------------------------------------------- */

/**
 * Clear framebuffer
 */
void ssd1306_clear(ssd1306_t *lcd);

/**
 * Draw pixel
 */
void ssd1306_draw_pixel(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    bool color
);

/**
 * Draw string (5x7 font)
 */
void ssd1306_draw_string(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *str
);

/* -------------------------------------------------------------------------- */
/*  UPDATE DISPLAY                                                           */
/* -------------------------------------------------------------------------- */

/**
 * Push framebuffer to OLED
 */
esp_err_t ssd1306_update(ssd1306_t *lcd);

/* -------------------------------------------------------------------------- */
/*  HIGH LEVEL HELPERS (WEB API FRIENDLY)                                   */
/* -------------------------------------------------------------------------- */

/**
 * Write text at line (0–7)
 */
esp_err_t ssd1306_write_line(
    ssd1306_t *lcd,
    uint8_t line,
    const char *text
);

/**
 * Print formatted text (printf-like)
 */
esp_err_t ssd1306_printf(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *fmt,
    ...
);

/**
 * Invert display
 */
esp_err_t ssd1306_set_invert(
    ssd1306_t *lcd,
    bool invert
);

/**
 * Reset display to default state
 */
esp_err_t ssd1306_reset_display(ssd1306_t *lcd);

esp_err_t ssd1306_write_line(
    ssd1306_t *lcd,
    uint8_t line,
    const char *text
);

esp_err_t ssd1306_reset_display(ssd1306_t *lcd);

esp_err_t ssd1306_set_invert(
    ssd1306_t *lcd,
    bool invert
);

esp_err_t ssd1306_printf(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *fmt,
    ...
);

extern ssd1306_t oled;

void ssd1306_draw_line(ssd1306_t *dev, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);

#ifdef __cplusplus
}
#endif
