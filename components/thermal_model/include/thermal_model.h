#ifndef THERMAL_MODEL_H
#define THERMAL_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * Modèle thermique simplifié type RC :
 *  - R : résistance thermique (K/W)
 *  - C : capacité thermique (J/K)
 *  - P : puissance de chauffe équivalente (W)
 *
 * On ne cherche pas la précision absolue, mais un modèle cohérent
 * qui permet de prédire la tendance de la température intérieure.
 */
typedef struct
{
    float R; // Résistance thermique (plus grand = mieux isolé)
    float C; // Capacité thermique (plus grand = maison inertielle)
    float P; // Puissance de chauffe équivalente (W)

    float last_Tint; // Dernière température intérieure observée
    float last_Text; // Dernière température extérieure observée
    int64_t last_ts; // Dernier timestamp (µs, esp_timer_get_time)

    bool initialized; // Le modèle a déjà été alimenté une fois
} thermal_model_t;

/**
 * Initialise le modèle thermique en mémoire.
 * Ne charge PAS NVS, il faut appeler thermal_model_load().
 */
void thermal_model_init(thermal_model_t *model);

/**
 * Charge le modèle thermique depuis NVS.
 * Si rien n’est stocké, le modèle est initialisé avec des valeurs par défaut.
 */
esp_err_t thermal_model_load(thermal_model_t *model);

/**
 * Sauvegarde le modèle thermique dans NVS.
 */
esp_err_t thermal_model_save(const thermal_model_t *model);

/**
 * Met à jour le modèle thermique à partir d’une nouvelle mesure.
 *
 * @param model       modèle thermique
 * @param Tint        température intérieure (°C)
 * @param Text        température extérieure (°C)
 * @param heating_on  true si le chauffage est ON, false sinon
 * @param timestamp_us timestamp courant en µs (esp_timer_get_time())
 */
void thermal_model_update(thermal_model_t *model,
                          float Tint,
                          float Text,
                          bool heating_on,
                          int64_t timestamp_us);

/**
 * Prédit la température intérieure dans dt_seconds secondes,
 * en supposant que l’état du chauffage (heating_on) reste constant.
 *
 * @param model       modèle thermique
 * @param Tint_now    température intérieure actuelle
 * @param Text_now    température extérieure actuelle
 * @param heating_on  état du chauffage supposé
 * @param dt_seconds  horizon de prédiction (ex : 3600 pour 1h)
 *
 * @return Tint prédite
 */
float thermal_model_predict(const thermal_model_t *model,
                            float Tint_now,
                            float Text_now,
                            bool heating_on,
                            float dt_seconds);

extern thermal_model_t g_thermal_model;

#endif // THERMAL_MODEL_H
