#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

/* -------------------------------------------------------------------------- */
/* CONSTANTES CONFIGURATION                                                   */
/* -------------------------------------------------------------------------- */
#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES  8  // 64 / 8 pixels par page

/* -------------------------------------------------------------------------- */
/* STRUCTURES DE DONNÉES                                                      */
/* -------------------------------------------------------------------------- */
typedef struct {
    const uint8_t *bitmap; // Pointeur vers le tableau de pixels brut
    uint8_t width;         // Largeur d'un caractère en pixels
    uint8_t height;        // Hauteur d'un caractère en pixels
    uint8_t spacing;       // Espace horizontal entre deux caractères
} ssd1306_font_t;

typedef struct {
    i2c_master_dev_handle_t dev;  // Handle du périphérique I2C (driver master)
    uint8_t address;               // Adresse I2C effective
    bool initialized;              // Statut du driver
    const ssd1306_font_t *font;    // Police courante sélectionnée
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES]; // Buffer graphique (1024 octets)
} ssd1306_t;

/* -------------------------------------------------------------------------- */
/* PROTOTYPES DES FONCTIONS PUBLIQUES                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialise la structure et configure la police par défaut.
 */
esp_err_t ssd1306_init(ssd1306_t *lcd, i2c_master_bus_handle_t bus, uint8_t address);

/**
 * @brief Enregistre le périphérique sur le bus I2C et envoie la séquence d'init matérielle.
 */
esp_err_t ssd1306_reinit(ssd1306_t *lcd, i2c_master_bus_handle_t bus, uint8_t address);

/**
 * @brief Éteint l'écran et détache proprement le périphérique du bus I2C.
 */
esp_err_t ssd1306_deinit(ssd1306_t *lcd);

/**
 * @brief Efface l'intégralité du buffer RAM local (met tout à zéro).
 */
void ssd1306_clear(ssd1306_t *lcd);

/**
 * @brief Transmet l'intégralité du buffer RAM local à l'écran.
 */
esp_err_t ssd1306_update(ssd1306_t *lcd);

/**
 * @brief Dessine ou efface un pixel unique dans le buffer local.
 */
void ssd1306_draw_pixel(ssd1306_t *lcd, uint8_t x, uint8_t y, bool color);

/**
 * @brief Trace une ligne entre deux points (Algorithme de Bresenham).
 */
// 🚀 Correction ici : changement de *dev en *lcd pour correspondre au .c
void ssd1306_draw_line(ssd1306_t *lcd, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);

/**
 * @brief Écrit une chaîne de caractères à des coordonnées précises.
 */
void ssd1306_draw_string(ssd1306_t *lcd, uint8_t x, uint8_t y, const char *str);

/**
 * @brief Efface une page de 8 pixels de haut et y écrit une chaîne de caractères.
 */
esp_err_t ssd1306_write_line(ssd1306_t *lcd, uint8_t line, const char *text);

/**
 * @brief Écrit une chaîne de caractères formatée (façon printf) à des coordonnées précises.
 */
esp_err_t ssd1306_printf(ssd1306_t *lcd, uint8_t x, uint8_t y, const char *fmt, ...) __attribute__ ((format (__printf__, 4, 5)));

/**
 * @brief Inverse l'affichage matériel.
 */
esp_err_t ssd1306_set_invert(ssd1306_t *lcd, bool invert);

/**
 * @brief Réinitialise l'état par défaut (Efface le buffer et rafraîchit l'écran).
 */
esp_err_t ssd1306_reset_display(ssd1306_t *lcd);

// 🚀 Instance globale partagée avec l'application
extern ssd1306_t oled;

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
