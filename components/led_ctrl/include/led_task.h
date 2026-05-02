// led_task.h
#pragma once
#include "led_types.h"
#include "esp_err.h"

// Déclarations des fonctions de la tâche LED
esp_err_t led_task_start(void);  // Exemple de fonction
/**
 * @brief Applique un effet fixe à la LED.
 * @param color Couleur RGB à appliquer.
 */
void led_effect_apply_fixed(led_color_t color);

/**
 * @brief Applique un effet "respiration" (fondu) à la LED.
 * @param color Couleur RGB de base.
 * @param step Étape actuelle pour le calcul du fondu.
 */
void led_effect_apply_breath(led_color_t color, uint32_t step);
