#ifndef THERMOSTAT_LEARNING_H
#define THERMOSTAT_LEARNING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Modèle interne du thermostat auto-apprenant
 * Inspiré du comportement Netatmo :
 *  - apprentissage des habitudes matin/soir
 *  - vitesse de chauffe réelle
 *  - vitesse de refroidissement
 *  - adaptation météo
 */
typedef struct
{
    float heat_rate;           // °C/min — vitesse de chauffe réelle
    float cool_rate;           // °C/min — vitesse de refroidissement
    float preferred_evening;   // consigne apprise entre 18h et 23h
    float preferred_morning;   // consigne apprise entre 6h et 9h
    float last_temp;           // dernière température connue
    uint32_t last_update;      // timestamp (sec) dernière mise à jour
} learning_model_t;

/**
 * Met à jour le modèle d’apprentissage
 * @param now_temp        température actuelle (arrondie comme dans thermostat.c)
 * @param consigne_user   consigne réellement utilisée par l’utilisateur
 */
void thermostat_learning_update(float now_temp, float consigne_user);

/**
 * Prédit la consigne idéale selon :
 *  - l’heure
 *  - les préférences apprises
 *  - la météo extérieure
 */
float thermostat_learning_predict_consigne(void);

/**
 * Retourne un JSON du modèle appris
 * (pour debug, API REST, interface web, etc.)
 */
char *thermostat_learning_get_json(void);

#endif // THERMOSTAT_LEARNING_H
