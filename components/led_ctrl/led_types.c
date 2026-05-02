/**
 * @file led_types.c
 * @brief Fonctions utilitaires pour les types définis dans led_types.h.
 */

#include "led_types.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "LED_TYPES";

// --- Fonctions utilitaires pour led_color_t ---

/**
 * @brief Compare deux couleurs RGB.
 * @param color1 Première couleur.
 * @param color2 Deuxième couleur.
 * @return true si les couleurs sont identiques, false sinon.
 */
bool led_color_equals(led_color_t color1, led_color_t color2) {
    return (color1.r == color2.r) && (color1.g == color2.g) && (color1.b == color2.b);
}

/**
 * @brief Convertit une couleur RGB en niveau de gris.
 * @param color Couleur RGB à convertir.
 * @return Valeur de gris (0-255).
 */
uint8_t led_color_to_grayscale(led_color_t color) {
    // Formule standard : 0.299*R + 0.587*G + 0.114*B
    return (uint8_t)(0.299f * color.r + 0.587f * color.g + 0.114f * color.b);
}

/**
 * @brief Inverse une couleur RGB.
 * @param color Couleur à inverser.
 * @return Couleur inversée.
 */
led_color_t led_color_invert(led_color_t color) {
    return (led_color_t){255 - color.r, 255 - color.g, 255 - color.b};
}

// --- Fonctions utilitaires pour stored_info_t et stored_alarm_t ---

/**
 * @brief Initialise une structure stored_info_t avec des valeurs par défaut.
 * @param info Pointeur vers la structure à initialiser.
 * @param name Nom de l'information.
 * @param color Couleur associée.
 */
void led_info_init(stored_info_t *info, const char *name, led_color_t color) {
    if (info) {
        strncpy(info->name, name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';  // Assure la terminaison
        info->color = color;
    }
}

/**
 * @brief Initialise une structure stored_alarm_t avec des valeurs par défaut.
 * @param alarm Pointeur vers la structure à initialiser.
 * @param name Nom de l'alarme.
 * @param blinks Nombre de clignotements.
 * @param color Couleur associée.
 */
void led_alarm_init(stored_alarm_t *alarm, const char *name, int blinks, led_color_t color) {
    if (alarm) {
        strncpy(alarm->name, name, sizeof(alarm->name) - 1);
        alarm->name[sizeof(alarm->name) - 1] = '\0';
        alarm->blinks = blinks;
        alarm->color = color;
    }
}
