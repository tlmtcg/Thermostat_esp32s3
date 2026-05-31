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
    uint8_t sht31_addr;                          // Adresse I2C (uint8_t)
    uint32_t sht31_read_interval_ms;             // Intervalle de lecture (uint32_t)
    bool sht31_log_to_sd;                       // Booléen converti en u8 (0 ou 1)

    // --- Jeedom ---
    bool jeedom_enabled;
    int32_t jeedom_id; // IMPORTANT : int32_t pour NVS

    // --- WiFi (optionnel) ---
    bool wifi_autoreconnect;

} runtime_config_t;

extern runtime_config_t g_cfg;

void config_runtime_load(void);
void config_runtime_save(void);
