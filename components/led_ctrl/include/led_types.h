/**
 * @file led_types.h
 * @brief Types et structures pour le module LED.
 * @note Ce fichier ne doit contenir que des déclarations de types.
 *       Les définitions de variables et fonctions doivent être dans les fichiers .c.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Modes de fonctionnement de la LED.
 */
typedef enum {
    LED_MODE_FIXED = 0,   ///< Mode fixe (couleur statique).
    LED_MODE_BREATH = 1   ///< Mode "respiration" (fondu de luminosité).
} led_mode_t;

/**
 * @brief Structure pour représenter une couleur RGB.
 */
typedef struct {
    uint8_t r;  ///< Composante rouge (0-255).
    uint8_t g;  ///< Composante verte (0-255).
    uint8_t b;  ///< Composante bleue (0-255).
} led_color_t;

/**
 * @brief Structure pour stocker une information de couleur (ex: ambiance).
 */
typedef struct {
    char name[32];       ///< Nom de l'ambiance (ex: "Ambiance Calme").
    led_color_t color;   ///< Couleur associée.
} stored_info_t;

/**
 * @brief Structure pour stocker une alarme (clignotements).
 */
typedef struct {
    char name[32];       ///< Nom de l'alarme (ex: "Alarme Urgente").
    int blinks;          ///< Nombre de clignotements.
    led_color_t color;   ///< Couleur de l'alarme.
} stored_alarm_t;

/**
 * @brief Structure pour un événement d'alarme (utilisé dans les queues FreeRTOS).
 */
typedef struct {
    int blinks;          ///< Nombre de clignotements.
    led_color_t color;   ///< Couleur de l'alarme.
} alarm_event_t;
