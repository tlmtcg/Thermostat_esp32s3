#include "prediction_engine.h"
#include "esp_log.h"
#include <math.h>
#include "cJSON.h"
#include "thermostat.h"
#include "weather.h"
#include "thermal_model.h"
#include "rc_estimator.h"
#include "relay.h"

static const char *TAG = "PRED_ENGINE";
thermal_runtime_t g_thermal_runtime = {0};

// Dernier résultat stocké
prediction_outputs_t g_last_pred = {0};
bool g_pred_valid = false;

static float clamp(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void prediction_engine_init(void)
{
    ESP_LOGI(TAG, "Prediction engine ready");
}

static float weather_code_effect(int code)
{
    if (code == 0)
        return -0.5f; // Soleil
    if (code <= 3)
        return -0.2f; // Peu nuageux
    if (code <= 48)
        return 0.0f; // Brouillard
    if (code <= 67)
        return +0.2f; // Pluie
    if (code <= 77)
        return +0.4f; // Neige
    if (code <= 82)
        return +0.3f; // Averses
    if (code <= 99)
        return +0.5f; // Orage
    return 0.0f;
}

void prediction_engine_compute(
    const thermal_model_t *model,
    float Tint_now,
    const prediction_inputs_t *inputs,
    prediction_outputs_t *out)
{
    if (!model || !inputs || !out)
        return;

    // 1️⃣ Prédictions thermiques RC
    out->Tint_1h = thermal_model_predict(model, Tint_now, inputs->temp_ext_now, false, 3600);
    out->Tint_3h = thermal_model_predict(model, Tint_now, inputs->temp_ext_now, false, 3 * 3600);
    out->Tint_6h = thermal_model_predict(model, Tint_now, inputs->temp_ext_now, false, 6 * 3600);

    // 2️⃣ Effet météo (ensoleillement)
    out->weather_effect = weather_code_effect(inputs->weather_code_1h);

    // 3️⃣ Effet humidité
    float delta_hum = inputs->humidity_1h - inputs->humidity_now;
    out->humidity_effect = clamp(delta_hum * 0.01f, -0.3f, +0.3f);

    // 4️⃣ Tendance météo (variation extérieure)
    float trend = inputs->temp_ext_3h - inputs->temp_ext_now;
    out->trend_effect = clamp(trend * -0.1f, -0.5f, +0.5f);

    // 5️⃣ Score global de besoin de chauffe
    float predicted_drop = Tint_now - out->Tint_1h;
    float score = 0.0f;

    score += predicted_drop * 0.3f;
    score += out->weather_effect * 0.4f;
    score += out->humidity_effect * 0.2f;
    score += out->trend_effect * 0.3f;

    out->heating_need_score = clamp(score, -1.0f, +1.0f);

    float tau_s = model->R * model->C;
    float tau_h = tau_s / 3600.0f;

    ESP_LOGI("THERMAL",
             "R=%.4f K/W, C=%.0f J/K, P=%.1f W, tau=%.0f s (%.1f h)",
             model->R, model->C, model->P, tau_s, tau_h);

    ESP_LOGI(TAG,
             "Pred Tint: +1h=%.2f +3h=%.2f +6h=%.2f | weather=%.2f hum=%.2f trend=%.2f | score=%.2f",
             out->Tint_1h, out->Tint_3h, out->Tint_6h,
             out->weather_effect, out->humidity_effect, out->trend_effect,
             out->heating_need_score);

    g_last_pred = *out;
    g_pred_valid = true;
}

char *prediction_engine_get_json_status(void)
{
    cJSON *root = cJSON_CreateObject();

    if (!g_pred_valid)
    {
        cJSON_AddStringToObject(root, "status", "no_data");
    }
    else
    {
        cJSON_AddStringToObject(root, "status", "ok");
        cJSON_AddNumberToObject(root, "Tint_1h", g_last_pred.Tint_1h);
        cJSON_AddNumberToObject(root, "Tint_3h", g_last_pred.Tint_3h);
        cJSON_AddNumberToObject(root, "Tint_6h", g_last_pred.Tint_6h);
        cJSON_AddNumberToObject(root, "weather_effect", g_last_pred.weather_effect);
        cJSON_AddNumberToObject(root, "humidity_effect", g_last_pred.humidity_effect);
        cJSON_AddNumberToObject(root, "trend_effect", g_last_pred.trend_effect);
        cJSON_AddNumberToObject(root, "heating_need_score", g_last_pred.heating_need_score);
        cJSON_AddNumberToObject(root, "Ta", g_thermal_runtime.Ta);
        cJSON_AddNumberToObject(root, "Tm", g_thermal_runtime.Tm);

        cJSON_AddNumberToObject(root, "Ra", g_thermal_runtime.Ra);
        cJSON_AddNumberToObject(root, "Rm", g_thermal_runtime.Rm);
        cJSON_AddNumberToObject(root, "Ca", g_thermal_runtime.Ca);
        cJSON_AddNumberToObject(root, "Cm", g_thermal_runtime.Cm);
        cJSON_AddNumberToObject(root, "P", g_thermal_runtime.P);

        cJSON_AddNumberToObject(root, "time_to_reach", g_thermal_runtime.time_to_reach);
        cJSON_AddNumberToObject(root, "start_heating_at", g_thermal_runtime.start_heating_at);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json; // malloc → libéré par l’API web
}

void prediction_engine_reset(void)
{
    memset(&g_last_pred, 0, sizeof(g_last_pred));
    g_pred_valid = false;
    ESP_LOGI(TAG, "Prediction engine reset");
}

void prediction_engine_force_recompute(void)
{
    // Ici tu peux déclencher un recalcul si tu veux
    ESP_LOGI(TAG, "Force recompute requested (not implemented yet)");
}

void prediction_engine_parse_command(const char *cmd)
{
    ESP_LOGI(TAG, "Prediction engine command: %s", cmd);
    // Tu peux ajouter des commandes plus tard
}

void prediction_engine_tick(void)
{
    // --- 1) Récupération météo ---
    float Text_now = weather_get_forecast_temp(0);
    float Text_1h = weather_get_forecast_temp(1);
    float Text_3h = weather_get_forecast_temp(3);
    float Text_6h = weather_get_forecast_temp(6);

    float H_now = weather_get_forecast_humidity(0);
    float H_1h = weather_get_forecast_humidity(1);

    int code_now = weather_get_current_code();
    int code_1h = weather_get_forecast_code(1);

    // --- 2) Température intérieure ---
    float Tint_now = g_thermostat_runtime.temperature;
    if (Tint_now < -50 || Tint_now > 80)
    {
        ESP_LOGW(TAG, "Temp intérieure invalide, tick ignoré");
        return;
    }

    // --- 3) Ancien moteur prédictif (on garde pour l’instant) ---
    prediction_inputs_t in = {
        .temp_ext_now = Text_now,
        .temp_ext_1h = Text_1h,
        .temp_ext_3h = Text_3h,
        .temp_ext_6h = Text_6h,

        .humidity_now = H_now,
        .humidity_1h = H_1h,

        .weather_code_now = code_now,
        .weather_code_1h = code_1h,
    };

    prediction_outputs_t out = {0};

    prediction_engine_compute(
        &g_thermal_model, // ancien modèle → sera supprimé plus tard
        Tint_now,
        &in,
        &out);

    g_last_pred = out;
    g_pred_valid = true;

    // --- 4) Nouveau modèle thermique 2R2C + EKF ---
    float u = get_relay_state() ? 1.0f : 0.0f;

    // Prédiction du modèle
    thermal_2r2c_predict(Text_now, u);

    // Mise à jour EKF avec la mesure réelle
    thermal_2r2c_update(Tint_now);

    // États (Ta/Tm)
    thermal_state_t st;
    thermal_2r2c_get_state(&st);

    g_thermal_runtime.Ta = st.Ta;
    g_thermal_runtime.Tm = st.Tm;

    // Paramètres thermiques (Ra/Rm/Ca/Cm/P)
    thermal_params_t prm;
    thermal_2r2c_get_params(&prm);

    g_thermal_runtime.Ra = prm.Ra;
    g_thermal_runtime.Rm = prm.Rm;
    g_thermal_runtime.Ca = prm.Ca;
    g_thermal_runtime.Cm = prm.Cm;
    g_thermal_runtime.P = prm.P;

    // --- 5) Temps pour atteindre la consigne ---
    float consigne = g_thermostat_runtime.effective_consigne;

    float tsec = thermal_2r2c_time_to_reach(consigne, Text_now);
    g_thermal_runtime.time_to_reach = tsec;

    // --- 6) Logs ---
    ESP_LOGI("2R2C", "Ta=%.2f Tm=%.2f", st.Ta, st.Tm);
    ESP_LOGI("2R2C", "Ra=%.2f Rm=%.2f Ca=%.2f Cm=%.2f P=%.2f",
             prm.Ra, prm.Rm, prm.Ca, prm.Cm, prm.P);

    ESP_LOGD(TAG,
             "Tick: Tint+1h=%.2f Tint+3h=%.2f Tint+6h=%.2f score=%.2f",
             out.Tint_1h, out.Tint_3h, out.Tint_6h, out.heating_need_score);
}

prediction_outputs_t prediction_engine_get_last(void)
{
    return g_last_pred;
}
