#ifndef HEATING_PROGRAM_H
#define HEATING_PROGRAM_H

#include "esp_err.h"
#include <stdint.h>

// Définitions de base
typedef enum { LUNDI=0, MARDI, MERCREDI, JEUDI, VENDREDI, SAMEDI, DIMANCHE, NB_JOURS } jour_t;
#define NB_PLAGES 4

// Structure pour un point précis (Heure/Min/Sec)
typedef struct {
    uint32_t secondes_minuit; 
    float temperature;
} point_consigne_t;

// Structure globale du planning
typedef struct {
    point_consigne_t planning[NB_JOURS][NB_PLAGES];
} chauffage_config_t;

// Variable globale du planning en mémoire vive
extern chauffage_config_t config;

// Fonctions
esp_err_t heating_init(chauffage_config_t *conf);
esp_err_t heating_save(const chauffage_config_t *conf);
void heating_set_point(chauffage_config_t *conf, jour_t j, int index, int h, int m, int s, float temp);
float heating_get_temp(const chauffage_config_t *conf, jour_t j, uint32_t now_sec);
void heating_reset_defaults(chauffage_config_t *conf);
char* heating_get_json(const chauffage_config_t *conf);
float heating_get_temp_current(const chauffage_config_t *conf);
const chauffage_config_t *heating_get_config(void);
#endif // HEATING_PROGRAM_H
