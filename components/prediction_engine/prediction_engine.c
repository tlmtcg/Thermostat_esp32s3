#include "prediction_engine.h"
#include "esp_log.h"
#include <math.h>
#include "cJSON.h"
#include "thermostat.h"
#include "weather.h"
#include "rc_estimator.h"
#include "relay.h"
#include "heating_program.h"
#include "time.h"
#include "time_utils.h"

static const char *TAG = "PRED_ENGINE";
thermal_runtime_t g_thermal_runtime = {0};

static float clamp(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

char *prediction_engine_get_json_status(void)
{
    cJSON *root = cJSON_CreateObject();

    // Toujours renvoyer "ok" tant que le modèle thermique tourne
    cJSON_AddStringToObject(root, "status", "ok");

    // États thermiques
    cJSON_AddNumberToObject(root, "Ta", g_thermal_runtime.Ta);
    cJSON_AddNumberToObject(root, "Tm", g_thermal_runtime.Tm);

    // Paramètres du modèle
    cJSON_AddNumberToObject(root, "Ra", g_thermal_runtime.Ra);
    cJSON_AddNumberToObject(root, "Rm", g_thermal_runtime.Rm);
    cJSON_AddNumberToObject(root, "Ca", g_thermal_runtime.Ca);
    cJSON_AddNumberToObject(root, "Cm", g_thermal_runtime.Cm);
    cJSON_AddNumberToObject(root, "P", g_thermal_runtime.P);

    // Chauffe prédictive
    cJSON_AddNumberToObject(root, "time_to_reach", g_thermal_runtime.time_to_reach);
    cJSON_AddNumberToObject(root, "start_heating_at", g_thermal_runtime.start_heating_at);

    // Consignes
    cJSON_AddNumberToObject(root, "effective_consigne", g_thermostat_runtime.effective_consigne);
    cJSON_AddNumberToObject(root, "next_consigne", g_thermostat_runtime.next_consigne);
    cJSON_AddNumberToObject(root, "next_consigne_ts", g_thermostat_runtime.next_consigne_ts);

    // Conversion JSON → string
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json; // libéré par l’API web
}

void prediction_engine_tick(void)
{
    int64_t ts = heating_program_get_next_target_timestamp();
    g_thermostat_runtime.next_consigne_ts = ts;

    if (ts > 0)
    {
        struct tm nt = time_utils_localtime_from_ts(ts);
        uint32_t sec_midnight = nt.tm_hour * 3600 + nt.tm_min * 60 + nt.tm_sec;

        g_thermostat_runtime.next_consigne =
            heating_get_temp(nt.tm_wday, sec_midnight);
    }
    else
    {
        g_thermostat_runtime.next_consigne = -1;
    }

    float Tint_now = g_thermostat_runtime.temperature;
    float Text_now = weather_get_forecast_temp(0);
    float u = get_relay_state() ? 1.0f : 0.0f;

    if (Tint_now < -50 || Tint_now > 80)
    {
        ESP_LOGW(TAG, "Temp intérieure invalide, tick ignoré");
        return;
    }

    // --- Nouveau modèle thermique ---
    thermal_2r2c_predict(Text_now, u);
    thermal_2r2c_update(Tint_now);

    thermal_state_t st;
    thermal_2r2c_get_state(&st);

    g_thermal_runtime.Ta = st.Ta;
    g_thermal_runtime.Tm = st.Tm;

    thermal_params_t prm;
    thermal_2r2c_get_params(&prm);

    g_thermal_runtime.Ra = prm.Ra;
    g_thermal_runtime.Rm = prm.Rm;
    g_thermal_runtime.Ca = prm.Ca;
    g_thermal_runtime.Cm = prm.Cm;
    g_thermal_runtime.P = prm.P;

    // --- Temps pour atteindre la consigne ---
    float consigne = g_thermostat_runtime.effective_consigne;
    g_thermal_runtime.time_to_reach =
        thermal_2r2c_time_to_reach(consigne, Text_now);

    // --- Allumage anticipé ---
    if (g_thermal_runtime.time_to_reach > 0)
    {
        int64_t target_ts = heating_program_get_next_target_timestamp();
        g_thermal_runtime.start_heating_at =
            target_ts - (int64_t)g_thermal_runtime.time_to_reach;
    }
    else
    {
        g_thermal_runtime.start_heating_at = -1;
    }

    ESP_LOGI("2R2C", "Ta=%.2f Tm=%.2f", st.Ta, st.Tm);
    ESP_LOGI("2R2C", "Ra=%.2f Rm=%.2f Ca=%.0f Cm=%.0f P=%.0f",
             prm.Ra, prm.Rm, prm.Ca, prm.Cm, prm.P);
}
