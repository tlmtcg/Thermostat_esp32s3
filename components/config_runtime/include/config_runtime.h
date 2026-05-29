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

    // --- Jeedom ---
    bool jeedom_enabled;
    int32_t jeedom_id; // IMPORTANT : int32_t pour NVS

    // --- WiFi (optionnel) ---
    bool wifi_autoreconnect;

} runtime_config_t;

extern runtime_config_t g_cfg;

void config_runtime_load(void);
void config_runtime_save(void);
