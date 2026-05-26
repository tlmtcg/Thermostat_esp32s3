#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum
{
    THERMOSTAT_MODE_AUTO = 0,
    THERMOSTAT_MODE_MANUAL,
    THERMOSTAT_MODE_ABSENT,
    THERMOSTAT_MODE_HORS_GEL,
    THERMOSTAT_MODE_LEARNING
} thermostat_mode_t;

typedef struct
{
    bool enabled;
    float consigne;
    thermostat_mode_t mode;
    float hysteresis;
    float calibration;
    bool frost_mode;
} thermostat_config_t;

typedef struct
{
    bool initialized;
    float effective_consigne;
    uint32_t change_count;
    char last_error[64];
    float temperature;        // Température intérieure actuelle (SHT31)
    float humidity;           // Humidité intérieure actuelle (SHT31)
    float temp_ext;           // Température extérieure actuelle
    float humidity_ext;       // AJOUT : Humidité extérieure actuelle (%)
    float temp_forecast_1h;   // AJOUT : Température prévue dans une heure (°C)
    bool state;               // État du relais (true = actif, false = inactif)
    bool temperature_valid;   // Etat du capteur (true = valid, false = invalid)
    float next_consigne;      // Prochaine consigne programmée (°C)
    int64_t next_consigne_ts; // Timestamp du prochain changement de consigne
} thermostat_runtime_t;

/* Alias temporaire pour compatibilite avec le code existant. */
typedef thermostat_config_t thermostat_state_t;

void thermostat_init(void);

esp_err_t thermostat_get_config(thermostat_config_t *out);

esp_err_t thermostat_set_config(const thermostat_config_t *config);

const thermostat_runtime_t *thermostat_get_runtime(void);

char *thermostat_get_json_status(void);

thermostat_state_t thermostat_get_state(void);

void thermostat_set_mode(thermostat_mode_t mode);

void thermostat_set_consigne(float value);

void thermostat_set_enabled(bool enabled);

void thermostat_update_current_consigne(void);

/**
 * @brief Met à jour les mesures intérieures issues du capteur SHT31
 * @param temp Température mesurée en °C
 * @param hum Humidité mesurée en %
 */
void thermostat_update_indoor_data(float temp, float hum, bool valid_temp);

/**
 * @brief Met à jour les données météo extérieures (ex: via une API ou sonde)
 * @param temp Température extérieure en °C
 * @param hum Humidité extérieure en %
 */
void thermostat_update_outdoor_data(float temp, float hum);

/**
 * @brief Met à jour la prévision météo
 * @param temp_1h Température attendue dans une heure en °C
 */
void thermostat_update_forecast_data(float temp_1h);

extern thermostat_runtime_t g_thermostat_runtime;

float thermal_2r2c_simulate_future(float horizon_sec, float Text, bool heating);

void must_heat();