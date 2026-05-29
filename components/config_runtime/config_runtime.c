#include "config_runtime.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

runtime_config_t g_cfg = {0};

void config_runtime_load(void)
{
    nvs_handle_t h;
    if (nvs_open("runtime", NVS_READWRITE, &h) != ESP_OK)
        return;

    size_t len = sizeof(g_cfg.weather_city);
    nvs_get_str(h, "city", g_cfg.weather_city, &len);

    size_t sz = sizeof(float);
    nvs_get_blob(h, "lat", &g_cfg.weather_lat, &sz);
    nvs_get_blob(h, "lon", &g_cfg.weather_lon, &sz);

    sz = sizeof(float);
    nvs_get_blob(h, "th_offset", &g_cfg.thermostat_offset, &sz);
    nvs_get_blob(h, "th_hyst", &g_cfg.thermostat_hysteresis, &sz);

    uint8_t b;
    if (nvs_get_u8(h, "th_auto", &b) == ESP_OK)
        g_cfg.thermostat_auto_mode = b;

    sz = sizeof(float);
    nvs_get_blob(h, "sht_tcal", &g_cfg.sht31_temp_calibration, &sz);
    nvs_get_blob(h, "sht_hcal", &g_cfg.sht31_hum_calibration, &sz);

    nvs_get_u8(h, "jee_en", &b);
    g_cfg.jeedom_enabled = b;

    nvs_get_i32(h, "jee_id", &g_cfg.jeedom_id);

    nvs_close(h);

    // Valeurs par défaut
    if (strlen(g_cfg.weather_city) == 0)
    {
        strcpy(g_cfg.weather_city, "Roncq");
        g_cfg.weather_lat = 50.75f;
        g_cfg.weather_lon = 3.12f;
    }
}

void config_runtime_save(void)
{
    nvs_handle_t h;
    if (nvs_open("runtime", NVS_READWRITE, &h) != ESP_OK)
        return;

    nvs_set_str(h, "city", g_cfg.weather_city);

    nvs_set_blob(h, "lat", &g_cfg.weather_lat, sizeof(float));
    nvs_set_blob(h, "lon", &g_cfg.weather_lon, sizeof(float));

    nvs_set_blob(h, "th_offset", &g_cfg.thermostat_offset, sizeof(float));
    nvs_set_blob(h, "th_hyst", &g_cfg.thermostat_hysteresis, sizeof(float));
    nvs_set_u8(h, "th_auto", g_cfg.thermostat_auto_mode);

    nvs_set_blob(h, "sht_tcal", &g_cfg.sht31_temp_calibration, sizeof(float));
    nvs_set_blob(h, "sht_hcal", &g_cfg.sht31_hum_calibration, sizeof(float));

    nvs_set_u8(h, "jee_en", g_cfg.jeedom_enabled);
    nvs_set_i32(h, "jee_id", g_cfg.jeedom_id);

    nvs_commit(h);
    nvs_close(h);
}
