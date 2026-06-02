#include "config_runtime.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_log.h"
#include "sht31.h"
#include "dht.h"

runtime_config_t g_cfg = {0};

void config_runtime_load(void)
{
    nvs_handle_t h;
    if (nvs_open("runtime", NVS_READWRITE, &h) != ESP_OK)
        return;

    // --- Chargement Météo ---
    size_t len = sizeof(g_cfg.weather_city);
    nvs_get_str(h, "city", g_cfg.weather_city, &len);

    size_t sz = sizeof(float);
    nvs_get_blob(h, "lat", &g_cfg.weather_lat, &sz);
    nvs_get_blob(h, "lon", &g_cfg.weather_lon, &sz);

    // --- Chargement Thermostat ---
    sz = sizeof(float);
    nvs_get_blob(h, "th_offset", &g_cfg.thermostat_offset, &sz);
    nvs_get_blob(h, "th_hyst", &g_cfg.thermostat_hysteresis, &sz);

    uint8_t b;
    if (nvs_get_u8(h, "th_auto", &b) == ESP_OK)
        g_cfg.thermostat_auto_mode = b;

    // --- Chargement Étalonnage SHT31 ---
    sz = sizeof(float);
    nvs_get_blob(h, "sht_tcal", &g_cfg.sht31_temp_calibration, &sz);
    nvs_get_blob(h, "sht_hcal", &g_cfg.sht31_hum_calibration, &sz);

    // --- Chargement Configuration Matérielle SHT31 ---
    if (nvs_get_u8(h, "sht_addr", &b) == ESP_OK)
    {
        g_cfg.sht31_addr = b;
    }
    else
    {
        g_cfg.sht31_addr = 0x44; // Adresse par défaut (SHT31 standard)
    }

    uint32_t interval;
    if (nvs_get_u32(h, "sht_int", &interval) == ESP_OK)
    {
        g_cfg.sht31_read_interval_ms = interval;
        ESP_LOGI("Config", "sht_int Interval %d", g_cfg.sht31_read_interval_ms);
    }
    else
    {
        g_cfg.sht31_read_interval_ms = 5000; // 5 secondes par défaut
    }
    ESP_LOGI("Config", "Interval %d", g_cfg.sht31_read_interval_ms);

    if (nvs_get_u8(h, "sht_log_sd", &b) == ESP_OK)
    {
        g_cfg.sht31_log_to_sd = (b != 0);
    }
    else
    {
        g_cfg.sht31_log_to_sd = false; // Désactivé par défaut
    }

    // --- Configuration DHT ---
    if (nvs_get_u8(h, "dht_pin", &b) == ESP_OK)
        g_cfg.dht_gpio_pin = b;
    else
        g_cfg.dht_gpio_pin = GPIO_NUM_7;

    if (nvs_get_u8(h, "dht_type", &b) == ESP_OK) 
        g_cfg.dht_sensor_type = b;
    else
        g_cfg.dht_sensor_type = 1;

    uint32_t interval_u32;
    if (nvs_get_u32(h, "dht_read_int_ms", &interval_u32) == ESP_OK)
    {    g_cfg.dht_read_int_ms = interval_u32;
        ESP_LOGI("Config", "DHT Interval enregistré : %d ms", g_cfg.dht_read_int_ms);}
    else
        g_cfg.dht_read_int_ms = 2000;
    
    if (nvs_get_u8(h, "dht_log_sd", &b) == ESP_OK)
        g_cfg.dht_log_to_sd = (b != 0);
    else
        g_cfg.dht_log_to_sd = false;

    // --- Chargement Domotique Jeedom ---
    if (nvs_get_u8(h, "jee_en", &b) == ESP_OK)
        g_cfg.jeedom_enabled = b;

    nvs_get_i32(h, "jee_id", &g_cfg.jeedom_id);

    nvs_close(h);

    // --- Valeurs par défaut globales (Météo) ---
    if (strlen(g_cfg.weather_city) == 0)
    {
        strcpy(g_cfg.weather_city, "Roncq");
        g_cfg.weather_lat = 50.75f;
        g_cfg.weather_lon = 3.12f;
    }

    // --- Application immédiate de la config SHT31 ---
    sht31_config_t cfg = {
        .addr = g_cfg.sht31_addr,
        .read_interval_ms = g_cfg.sht31_read_interval_ms,
        .log_to_sd = g_cfg.sht31_log_to_sd
    };

    sht31_set_config(&cfg);
}

void config_runtime_save(void)
{
    nvs_handle_t h;
    if (nvs_open("runtime", NVS_READWRITE, &h) != ESP_OK)
        return;

    // --- Configuration Météo ---
    nvs_set_str(h, "city", g_cfg.weather_city);
    nvs_set_blob(h, "lat", &g_cfg.weather_lat, sizeof(float));
    nvs_set_blob(h, "lon", &g_cfg.weather_lon, sizeof(float));

    // --- Configuration Thermostat ---
    nvs_set_blob(h, "th_offset", &g_cfg.thermostat_offset, sizeof(float));
    nvs_set_blob(h, "th_hyst", &g_cfg.thermostat_hysteresis, sizeof(float));
    nvs_set_u8(h, "th_auto", g_cfg.thermostat_auto_mode);

    // --- Configuration Étalonnage SHT31 ---
    nvs_set_blob(h, "sht_tcal", &g_cfg.sht31_temp_calibration, sizeof(float));
    nvs_set_blob(h, "sht_hcal", &g_cfg.sht31_hum_calibration, sizeof(float));

    // --- Configuration Matérielle & Runtime SHT31 ---
    nvs_set_u8(h, "sht_addr", g_cfg.sht31_addr);
    nvs_set_u32(h, "sht_int", g_cfg.sht31_read_interval_ms);
    ESP_LOGI("Config sht", "Interval %d", g_cfg.sht31_read_interval_ms);

    nvs_set_u8(h, "sht_log_sd", g_cfg.sht31_log_to_sd ? 1 : 0);

    // --- Configuration DHT ---
    nvs_set_u8(h, "dht_pin", g_cfg.dht_gpio_pin);
    nvs_set_u8(h, "dht_type", g_cfg.dht_sensor_type);
    nvs_set_u32(h, "dht_read_int_ms", g_cfg.dht_read_int_ms);
    ESP_LOGI("Config dht", "Interval %d", g_cfg.dht_read_int_ms);
    nvs_set_u8(h, "dht_log_sd", g_cfg.dht_log_to_sd ? 1 : 0);

    // --- Configuration Domotique Jeedom ---
    nvs_set_u8(h, "jee_en", g_cfg.jeedom_enabled);
    nvs_set_i32(h, "jee_id", g_cfg.jeedom_id);

    // Enregistrement effectif et fermeture
    nvs_commit(h);
    nvs_close(h);
}