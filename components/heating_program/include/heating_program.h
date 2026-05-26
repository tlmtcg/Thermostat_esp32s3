#ifndef HEATING_PROGRAM_H
#define HEATING_PROGRAM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
 * CONSTANTES
 * ========================================================= */
#define NB_JOURS 7
#define NB_PLAGES 4

/* =========================================================
 * TYPES
 * ========================================================= */

/**
 * @brief Structure d'une plage horaire
 */
typedef struct {
    uint32_t secondes_minuit;   // Heure en secondes depuis minuit
    float temperature;          // Température consigne
} plage_t;

/**
 * @brief Configuration complète du chauffage
 */
typedef struct {
    plage_t planning[NB_JOURS][NB_PLAGES];

    // (optionnel futur : flags globaux)
    bool enabled;
} chauffage_config_t;

/* =========================================================
 * ENUMS JOUR
 * ========================================================= */
typedef enum {
    JOUR_DIMANCHE = 0,
    JOUR_LUNDI,
    JOUR_MARDI,
    JOUR_MERCREDI,
    JOUR_JEUDI,
    JOUR_VENDREDI,
    JOUR_SAMEDI
} jour_t;

/* =========================================================
 * API PLANNING
 * ========================================================= */

/**
 * @brief Initialise le système chauffage (RAM + NVS)
 */
esp_err_t heating_init();

/**
 * @brief Sauvegarde la config dans la NVS
 */
esp_err_t heating_save();

/**
 * @brief Reset valeurs par défaut en RAM
 */
void heating_reset_defaults();

/**
 * @brief Définit un point de consigne dans le planning
 */
esp_err_t heating_set_point(jour_t j,
                       int index,
                       int h,
                       int m,
                       int s,
                       float temp);

/**
 * @brief Température selon planning
 */
float heating_get_temp(
                       jour_t j,
                       uint32_t now_sec);

/* =========================================================
 * TEMPERATURE LIVE
 * ========================================================= */

/**
 * @brief Température actuelle selon heure système
 */
float heating_get_temp_current();

/* =========================================================
 * JSON EXPORT
 * ========================================================= */

/**
 * @brief Export planning en JSON (malloc -> free requis)
 */
char *heating_get_json();

/* =========================================================
 * CONFIG GLOBALE (IMPORTANT)
 * ========================================================= */

/**
 * @brief Accès lecture seule à la config globale
 */
const chauffage_config_t *heating_get_config(void);

/**
 * @brief Accès lecture/écriture à la config globale
 */
chauffage_config_t *heating_get_config_rw(void);

esp_err_t heating_get_program_json(char **out_json);
esp_err_t heating_reset_program(void);

int64_t heating_program_get_next_target_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif // HEATING_PROGRAM_H
