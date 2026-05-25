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
    // Vérification des données météo
    float t_ext_now = weather_get_forecast_temp(0);
    float t_ext_1h = weather_get_forecast_temp(1);
    float t_ext_3h = weather_get_forecast_temp(3);
    float t_ext_6h = weather_get_forecast_temp(6);

    float h_ext_now = weather_get_forecast_humidity(0);
    float h_ext_1h = weather_get_forecast_humidity(1);

    int code_now = weather_get_current_code();
    int code_1h = weather_get_forecast_code(1);

    // Vérification température intérieure
    float Tint_now = g_thermostat_runtime.temperature;
    if (Tint_now < -50 || Tint_now > 80)
    {
        ESP_LOGW(TAG, "Temp intérieure invalide, tick ignoré");
        return;
    }

    // Construction des entrées
    prediction_inputs_t in = {
        .temp_ext_now = t_ext_now,
        .temp_ext_1h = t_ext_1h,
        .temp_ext_3h = t_ext_3h,
        .temp_ext_6h = t_ext_6h,

        .humidity_now = h_ext_now,
        .humidity_1h = h_ext_1h,

        .weather_code_now = code_now,
        .weather_code_1h = code_1h,
    };

    prediction_outputs_t out = {0};

    // Calcul
    prediction_engine_compute(
        &g_thermal_model,
        Tint_now,
        &in,
        &out);

    // Stockage pour l’API web
    g_last_pred = out;
    g_pred_valid = true;

    static float last_Tint = NAN;

    float Text_now = weather_get_forecast_temp(0);
    float u = get_relay_state() ? 1.0f : 0.0f;

    last_Tint = Tint_now;

    thermal_2r2c_predict(Text_now, u);
    thermal_2r2c_update(Tint_now);

    thermal_state_t st;
    thermal_2r2c_get_state(&st);

    ESP_LOGI("2R2C", "Ta=%.2f Tm=%.2f", st.Ta, st.Tm);

    ESP_LOGD(TAG,
             "Tick: Tint+1h=%.2f Tint+3h=%.2f Tint+6h=%.2f score=%.2f",
             out.Tint_1h, out.Tint_3h, out.Tint_6h, out.heating_need_score);
}

prediction_outputs_t prediction_engine_get_last(void)
{
    return g_last_pred;
}
