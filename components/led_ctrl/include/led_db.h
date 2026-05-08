/**
 * @file led_db.h
 * @brief Interface pour la gestion de la base de données LED.
 *
 * Pipeline LED A :
 *  - Stocke les ambiances (infos) et les alarmes (blinks + couleurs)
 *  - Fournit les index et les données à alert_manager et led_task
 *  - Simule ambiance + alarme via led_set_background() + alert_add()
 */

#pragma once

#include "led_types.h"
#include "esp_err.h"
#include <stdbool.h>

/* ============================================================================
   POUR ÉVITER LES DÉPENDANCES CIRCULAIRES
   ============================================================================ */

/**
 * @brief Fonction définie dans led_ctrl.c, utilisée par led_db_internal_simulate().
 */
extern void led_set_background(led_mode_t mode, led_color_t color, int speed_ms);

/* ============================================================================
   FONCTIONS INTERNES (utilisées par led_ctrl, alert_manager, simulate)
   ============================================================================ */

/**
 * @brief Ajoute une ambiance (info) dans la DB.
 */
void led_db_internal_add_info(const char *name, led_color_t color);

/**
 * @brief Ajoute une alarme dans la DB.
 */
void led_db_internal_add_alarm(const char *name, int blinks, led_color_t color);

/**
 * @brief Supprime une entrée (info ou alarme) par son nom.
 */
void led_db_internal_delete_by_name(const char *name);

/**
 * @brief Affiche l'état de la DB dans les logs.
 */
void led_db_internal_print_status(void);

/**
 * @brief Nombre d'ambiances stockées.
 */
int led_db_internal_get_info_count(void);

/**
 * @brief Nombre d'alarmes stockées.
 */
int led_db_internal_get_alarm_count(void);

/**
 * @brief Récupère une ambiance par index.
 */
stored_info_t *led_db_internal_get_info_by_idx(int idx);

/**
 * @brief Récupère une alarme par index.
 */
stored_alarm_t *led_db_internal_get_alarm_by_idx(int idx);

/**
 * @brief Simule une ambiance + une alarme.
 *        - info_idx >= 0 → applique le fond
 *        - alarm_idx >= 0 → alert_add(name)
 *        - alarm_idx == -1 → alert_clear_all()
 */
void led_db_internal_simulate(int info_idx, int alarm_idx);

/* ============================================================================
   FONCTIONS PUBLIQUES
   ============================================================================ */

/**
 * @brief Initialise la DB (charge depuis stockage).
 */
void led_db_init(void);

/**
 * @brief Sauvegarde la DB dans le stockage.
 */
void led_db_save(void);

/**
 * @brief Charge la DB depuis le stockage.
 */
void led_db_load(void);

/**
 * @brief Vérifie si une entrée existe (info ou alarme).
 */
bool led_db_exists(const char *name);

/**
 * @brief Renvoie l'index d'une ambiance par son nom.
 */
int led_db_get_info_idx_by_name(const char *name);

/**
 * @brief Renvoie l'index d'une alarme par son nom.
 */
int led_db_get_alarm_idx_by_name(const char *name);
