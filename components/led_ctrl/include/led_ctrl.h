/**
 * @file led_ctrl.h
 * @brief Interface publique du contrôleur LED (pipeline A).
 *
 * Ce module orchestre :
 *  - le fond (fixed / breath)
 *  - les alarmes (via alert_manager)
 *  - la base de données LED (led_db)
 *  - la tâche d'affichage (led_task)
 */

#pragma once

#include "led_types.h"
#include "esp_err.h"

/* ============================================================================
   INITIALISATION / STOP
   ============================================================================ */

/**
 * @brief Initialise tout le pipeline LED :
 *  - stockage
 *  - driver LED
 *  - base de données LED
 *  - alert_manager
 *  - tâche LED
 */
esp_err_t led_init(void);

/**
 * @brief Stoppe la LED et remet le fond à OFF.
 */
void led_stop(void);

/* ============================================================================
   BACKGROUND (FOND)
   ============================================================================ */

/**
 * @brief Définit le fond de la LED (fixed ou breath).
 *
 * @param mode      LED_MODE_FIXED ou LED_MODE_BREATH
 * @param color     Couleur RGB
 * @param speed_ms  Vitesse du breath (ignored en mode fixed)
 */
void led_set_background(led_mode_t mode, led_color_t color, int speed_ms);

/* ============================================================================
   BASE DE DONNÉES LED (délégation vers led_db.c)
   ============================================================================ */

void led_db_add_info(const char *name, led_color_t color);
void led_db_add_alarm(const char *name, int blinks, led_color_t color);
void led_db_delete_by_name(const char *name);
void led_db_print_status(void);

int led_db_get_info_count(void);
int led_db_get_alarm_count(void);

stored_info_t *led_db_get_info_by_idx(int idx);
stored_alarm_t *led_db_get_alarm_by_idx(int idx);

/**
 * @brief Simule une ambiance + une alarme.
 *        - info_idx >= 0 → applique le fond
 *        - alarm_idx >= 0 → alert_add(name)
 *        - alarm_idx == -1 → alert_clear_all()
 */
void led_db_simulate(int info_idx, int alarm_idx);
