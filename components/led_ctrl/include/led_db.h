/**
 * @file led_db.h
 * @brief Interface pour la gestion de la base de données des configurations LED.
 * @note Ce header déclare les fonctions internes et publiques pour la base de données.
 */

#pragma once

// Inclure les types partagés
#include "led_types.h"
#include "esp_err.h"

// =============================================================================
// DÉCLARATIONS EXTERNES (pour éviter les dépendances circulaires avec led_ctrl.h)
// =============================================================================
// Ces fonctions sont définies dans led_ctrl.c, mais utilisées ici.
// On les déclare avec 'extern' pour éviter d'inclure led_ctrl.h.

extern void led_set_background(led_mode_t mode, led_color_t color, int speed_ms);
extern void led_add_alarm(int nb_blinks, led_color_t color);

// =============================================================================
// FONCTIONS INTERNES (appelées depuis led_ctrl.c)
// =============================================================================
// Ces fonctions sont utilisées par led_ctrl.c pour interagir avec la base de données.
// Elles sont préfixées par 'internal_' pour éviter les conflits de noms.

/**
 * @brief Ajoute une information (ambiance) à la base de données.
 * @param name Nom de l'information.
 * @param color Couleur RGB associée.
 */
void led_db_internal_add_info(const char *name, led_color_t color);

/**
 * @brief Ajoute une alarme à la base de données.
 * @param name Nom de l'alarme.
 * @param blinks Nombre de clignotements.
 * @param color Couleur RGB associée.
 */
void led_db_internal_add_alarm(const char *name, int blinks, led_color_t color);

/**
 * @brief Supprime une entrée (info ou alarme) de la base de données par son nom.
 * @param name Nom de l'entrée à supprimer.
 */
void led_db_internal_delete_by_name(const char *name);

/**
 * @brief Affiche l'état actuel de la base de données (logs).
 */
void led_db_internal_print_status(void);

/**
 * @brief Récupère le nombre d'informations (ambiances) en mémoire.
 * @return Nombre d'informations.
 */
int led_db_internal_get_info_count(void);

/**
 * @brief Récupère le nombre d'alarmes en mémoire.
 * @return Nombre d'alarmes.
 */
int led_db_internal_get_alarm_count(void);

/**
 * @brief Récupère une information par son index.
 * @param idx Index de l'information (0 à led_db_internal_get_info_count()-1).
 * @return Pointeur vers l'information, ou NULL si l'index est invalide.
 */
stored_info_t *led_db_internal_get_info_by_idx(int idx);

/**
 * @brief Récupère une alarme par son index.
 * @param idx Index de l'alarme (0 à led_db_internal_get_alarm_count()-1).
 * @return Pointeur vers l'alarme, ou NULL si l'index est invalide.
 */
stored_alarm_t *led_db_internal_get_alarm_by_idx(int idx);

/**
 * @brief Simule une ambiance et/ou une alarme à partir de la base de données.
 * @param info_idx Index de l'information (ambiance) à simuler (-1 pour ignorer).
 * @param alarm_idx Index de l'alarme à simuler (-1 pour ignorer).
 */
void led_db_internal_simulate(int info_idx, int alarm_idx);

// =============================================================================
// FONCTIONS PUBLIQUES (pour l'initialisation et le stockage)
// =============================================================================

/**
 * @brief Initialise la base de données (charge les données depuis le stockage).
 */
void led_db_init(void);

/**
 * @brief Sauvegarde la base de données dans le stockage (SPIFFS).
 */
void led_db_save(void);

/**
 * @brief Charge la base de données depuis le stockage (SPIFFS).
 */
void led_db_load(void);

bool led_db_exists(const char *name);

int led_db_get_info_idx_by_name(const char *name);

int led_db_get_alarm_idx_by_name(const char *name);
