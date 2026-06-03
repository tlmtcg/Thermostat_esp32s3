#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{

    // --- Météo ---
    char weather_city[32];
    float weather_lat;
    float weather_lon;

    // --- Thermostat ---
    float thermostat_offset;
    float thermostat_hysteresis;
    bool thermostat_auto_mode;

    // --- Capteurs SHT31 ---
    float sht31_temp_calibration;
    float sht31_hum_calibration;
    // --- Ajout : Configuration Matérielle & Runtime SHT31 ---
    uint8_t sht31_addr;              // Adresse I2C (uint8_t)
    uint32_t sht31_read_interval_ms; // Intervalle de lecture (uint32_t)
    bool sht31_log_to_sd;            // Booléen converti en u8 (0 ou 1)

    // --- CONFIGURATION DHT AJOUTÉE ---
    int dht_gpio_pin;
    int dht_sensor_type;
    uint32_t dht_read_int_ms;
    bool dht_log_to_sd;

    // --- Configuration Mode Critique & Secours ---
    uint32_t secu_cycle_duration_sec; // Durée totale de la période PWM (ex: 7200 pour 2h)
    uint32_t fallback_duty_percent;   // % de chauffe fixe si double panne (ex: 30)
    float extreme_ext_temp;           // Seuil de température extérieure pour 100% de chauffe (ex: -10.0f)

    // --- Jeedom ---
    bool jeedom_enabled;
    int32_t jeedom_id; // IMPORTANT : int32_t pour NVS

    // --- WiFi (optionnel) ---
    bool wifi_autoreconnect;

} runtime_config_t;

extern runtime_config_t g_cfg;

void config_runtime_load(void);
void config_runtime_save(void);
