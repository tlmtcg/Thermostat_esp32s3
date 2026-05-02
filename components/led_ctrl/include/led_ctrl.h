/**
 * @file led_ctrl.h
 * @brief Interface publique du contrôleur LED.
 * @note Ce header déclare les fonctions publiques pour contrôler la LED et interagir avec la base de données.
 */

#pragma once

// Inclure les types partagés
#include "led_types.h"
#include "esp_err.h"

// =============================================================================
// FONCTIONS DE CONTRÔLE DE LA LED
// =============================================================================

/**
 * @brief Initialise le module LED (driver, stockage, base de données, tâche).
 * @return ESP_OK en cas de succès, une erreur ESP-IDF sinon.
 */
esp_err_t led_init(void);

/**
 * @brief Éteint la LED et réinitialise son état.
 */
void led_stop(void);

/**
 * @brief Définit l'état de fond (background) de la LED.
 * @param mode Mode de fonctionnement (LED_MODE_FIXED ou LED_MODE_BREATH).
 * @param color Couleur RGB à appliquer.
 * @param speed_ms Vitesse pour les effets dynamiques (non utilisé en mode FIXED).
 */
void led_set_background(led_mode_t mode, led_color_t color, int speed_ms);

/**
 * @brief Ajoute une alarme (clignotements) à la file d'attente.
 * @param nb_blinks Nombre de clignotements.
 * @param color Couleur RGB pour l'alarme.
 */
void led_add_alarm(int nb_blinks, led_color_t color);

/**
 * @brief Efface toutes les alarmes en attente et réinitialise la LED.
 */
void led_clear_alarms(void);

/**
 * @brief Définit un effet personnalisé (étendable pour de futurs modes).
 * @param mode Mode de fonctionnement.
 * @param color Couleur RGB.
 * @param count Nombre de répétitions (non utilisé actuellement).
 * @param speed_ms Vitesse de l'effet.
 */
void led_set_effect(led_mode_t mode, led_color_t color, int count, int speed_ms);

// =============================================================================
// FONCTIONS DE LA BASE DE DONNÉES (Délégation vers led_db.c)
// =============================================================================

/**
 * @brief Ajoute une information (ambiance) à la base de données.
 * @param name Nom de l'information.
 * @param color Couleur RGB associée.
 */
void led_db_add_info(const char *name, led_color_t color);

/**
 * @brief Ajoute une alarme à la base de données.
 * @param name Nom de l'alarme.
 * @param blinks Nombre de clignotements.
 * @param color Couleur RGB associée.
 */
void led_db_add_alarm(const char *name, int blinks, led_color_t color);

/**
 * @brief Supprime une entrée (info ou alarme) de la base de données par son nom.
 * @param name Nom de l'entrée à supprimer.
 */
void led_db_delete_by_name(const char *name);

/**
 * @brief Affiche l'état actuel de la base de données (logs).
 */
void led_db_print_status(void);

/**
 * @brief Récupère le nombre d'informations (ambiances) en mémoire.
 * @return Nombre d'informations.
 */
int led_db_get_info_count(void);

/**
 * @brief Récupère le nombre d'alarmes en mémoire.
 * @return Nombre d'alarmes.
 */
int led_db_get_alarm_count(void);

/**
 * @brief Récupère une information par son index.
 * @param idx Index de l'information (0 à led_db_get_info_count()-1).
 * @return Pointeur vers l'information, ou NULL si l'index est invalide.
 */
stored_info_t *led_db_get_info_by_idx(int idx);

/**
 * @brief Récupère une alarme par son index.
 * @param idx Index de l'alarme (0 à led_db_get_alarm_count()-1).
 * @return Pointeur vers l'alarme, ou NULL si l'index est invalide.
 */
stored_alarm_t *led_db_get_alarm_by_idx(int idx);

/**
 * @brief Simule une ambiance et/ou une alarme à partir de la base de données.
 * @param info_idx Index de l'information (ambiance) à simuler (-1 pour ignorer).
 * @param alarm_idx Index de l'alarme à simuler (-1 pour ignorer).
 */
void led_db_simulate(int info_idx, int alarm_idx);