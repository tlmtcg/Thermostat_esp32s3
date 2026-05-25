#include "thermostat.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#include "alert_manager.h"
#include "esp_log.h"
#include "thermostat_storage.h"
#include "heating_program.h"
#include "weather.h"
#include "app_context.h"
#include "relay.h"
#include <math.h>
#include "thermostat_learning.h"
#include "thermal_model.h"
#include "time_utils.h"
#include "predict_adjustment.h"
#include "rc_estimator.h"

static const char *TAG = "THERMOSTAT";

static thermostat_config_t g_thermostat_config;

thermostat_runtime_t g_thermostat_runtime = {
    .last_error = "",
};

static thermostat_config_t thermostat_default_config(void)
{
    thermostat_config_t config = {
        .enabled = true,
        .consigne = 21.0f,
        .mode = THERMOSTAT_MODE_HORS_GEL,
        .hysteresis = 0.3f,
        .calibration = 0.0f,
        .frost_mode = false,
    };

    return config;
}

static float thermostat_clamp_consigne(float value)
{
    if (value < 5.0f)
        return 5.0f;

    if (value > 35.0f)
        return 35.0f;

    return value;
}
/**
 * Arrondit un float à N décimales
 */
static float round_float(float value, uint8_t decimals)
{
    float factor = 1.0f;

    for (uint8_t i = 0; i < decimals; i++)
    {
        factor *= 10.0f;
    }

    return roundf(value * factor) / factor;
}
/**
 * Calcule la consigne réellement utilisée selon le mode actif
 */
void thermostat_update_current_consigne(void)
{
    static thermostat_mode_t last_mode = (thermostat_mode_t)-1;

    // Mise à jour runtime
    g_thermostat_runtime.temperature = round_float(g_ctx.temperature, 2);
    g_thermostat_runtime.state = get_relay_state();

    float consigne_auto = heating_get_temp_current();

    switch (g_thermostat_config.mode)
    {
    // -------------------------
    // MODE MANUEL
    // -------------------------
    case THERMOSTAT_MODE_MANUAL:
        g_thermostat_runtime.effective_consigne = g_thermostat_config.consigne;

        if (last_mode != THERMOSTAT_MODE_MANUAL)
            ESP_LOGI(TAG, "Consigne manu %.2f", g_thermostat_runtime.effective_consigne);

        last_mode = THERMOSTAT_MODE_MANUAL;
        break;

    // -------------------------
    // MODE AUTO (corrigé)
    // -------------------------
    case THERMOSTAT_MODE_AUTO:
    {
        // 1. Récupérer la dernière prédiction calculée par prediction_engine_tick()
        prediction_outputs_t pred_out = prediction_engine_get_last();

        // 2. Ajustement intelligent basé sur la prédiction
        predict_adjustment_inputs_t adj_in = {
            .consigne_auto = consigne_auto,
            .Tint_now = g_thermostat_runtime.temperature,
            .Text_now = g_thermostat_runtime.temp_ext,
        };

        float adj = predict_adjustment_compute(&g_thermal_model, &pred_out, &adj_in);

        // 3. Consigne finale
        g_thermostat_runtime.effective_consigne = consigne_auto + adj;

        last_mode = THERMOSTAT_MODE_AUTO;
        break;
    }

    // -------------------------
    // MODE ABSENT
    // -------------------------
    case THERMOSTAT_MODE_ABSENT:
        g_thermostat_runtime.effective_consigne = consigne_auto - 4.0f;

        if (last_mode != THERMOSTAT_MODE_ABSENT)
            ESP_LOGI(TAG, "Consigne absent %.2f", g_thermostat_runtime.effective_consigne);

        last_mode = THERMOSTAT_MODE_ABSENT;
        break;

    // -------------------------
    // MODE HORS GEL
    // -------------------------
    case THERMOSTAT_MODE_HORS_GEL:
    {
        const float SEUIL_GEL_EXT = 2.0f;
        const float SEUIL_VIGILANCE_INT = 7.0f;
        const float CONSIGNE_MIN_ECO = 5.0f;
        const float CONSIGNE_BOOST_HG = 8.5f;

        float ext_temp = temperature_get_outdoor();
        float int_temp = g_thermostat_runtime.temperature;
        bool ext_temp_valide = true;

        if (isnan(ext_temp) || ext_temp < -40.0f || ext_temp > 60.0f)
        {
            ext_temp_valide = false;
            ext_temp = -10.0f;
        }

        if (ext_temp <= SEUIL_GEL_EXT && int_temp <= SEUIL_VIGILANCE_INT)
            g_thermostat_runtime.effective_consigne = CONSIGNE_BOOST_HG;
        else
            g_thermostat_runtime.effective_consigne = CONSIGNE_MIN_ECO;

        static float last_applied_consigne = -100.0f;
        if (last_mode != THERMOSTAT_MODE_HORS_GEL ||
            g_thermostat_runtime.effective_consigne != last_applied_consigne)
        {
            if (!ext_temp_valide)
                ESP_LOGW(TAG, "ALERTE : Capteur Extérieur INDISPONIBLE ! Sécurité active. Int: %.1f°C -> Consigne: %.1f°C",
                         int_temp, g_thermostat_runtime.effective_consigne);
            else
                ESP_LOGI(TAG, "HG AFINÉ - Ext: %.1f°C, Int: %.1f°C -> Consigne: %.1f°C",
                         ext_temp, int_temp, g_thermostat_runtime.effective_consigne);

            last_applied_consigne = g_thermostat_runtime.effective_consigne;
        }

        last_mode = THERMOSTAT_MODE_HORS_GEL;
        break;
    }

    // -------------------------
    // MODE LEARNING
    // -------------------------
    case THERMOSTAT_MODE_LEARNING:
        thermostat_learning_update(g_thermostat_runtime.temperature,
                                   g_thermostat_config.consigne);

        g_thermostat_runtime.effective_consigne =
            thermostat_learning_predict_consigne();

        ESP_LOGI(TAG, "Learning consigne %.2f",
                 g_thermostat_runtime.effective_consigne);

        last_mode = THERMOSTAT_MODE_LEARNING;
        break;

    // -------------------------
    // MODE PAR DÉFAUT
    // -------------------------
    default:
        g_thermostat_runtime.effective_consigne = g_thermostat_config.consigne;
        break;
    }

    // Clamp final
    g_thermostat_runtime.effective_consigne =
        thermostat_clamp_consigne(g_thermostat_runtime.effective_consigne);
}

static void thermostat_sync_alerts(void)
{
    if (g_thermostat_config.enabled)
    {
        alert_remove("Thermostat DESACTIVE");
    }
    else
    {
        alert_add("Thermostat DESACTIVE");
    }

    if (g_thermostat_config.mode == THERMOSTAT_MODE_HORS_GEL ||
        g_thermostat_config.frost_mode)
    {
        alert_add("Mode hors-gel actif");
    }
    else
    {
        alert_remove("Mode hors-gel actif");
    }
}

static esp_err_t thermostat_save_config(void)
{
    esp_err_t err = thermostat_storage_save(&g_thermostat_config);

    if (err != ESP_OK)
    {
        snprintf(g_thermostat_runtime.last_error,
                 sizeof(g_thermostat_runtime.last_error),
                 "Save failed: %s",
                 esp_err_to_name(err));

        ESP_LOGE(TAG, "%s", g_thermostat_runtime.last_error);
        return err;
    }

    g_thermostat_runtime.last_error[0] = '\0';
    g_thermostat_runtime.change_count++;

    ESP_LOGI(TAG,
             "Saved: mode=%d consigne=%.1f enabled=%d",
             g_thermostat_config.mode,
             g_thermostat_config.consigne,
             g_thermostat_config.enabled);

    return ESP_OK;
}

void thermostat_init(void)
{
    esp_err_t err = thermostat_storage_load(&g_thermostat_config);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No valid saved config, using defaults");
        g_thermostat_config = thermostat_default_config();
        thermostat_storage_save(&g_thermostat_config);
    }

    g_thermostat_runtime.initialized = true;
    thermostat_sync_alerts();

    ESP_LOGI(TAG,
             "Loaded: mode=%d consigne=%.1f enabled=%d",
             g_thermostat_config.mode,
             g_thermostat_config.consigne,
             g_thermostat_config.enabled);
}

esp_err_t thermostat_get_config(thermostat_config_t *out)
{
    if (!out)
        return ESP_ERR_INVALID_ARG;

    *out = g_thermostat_config;
    return ESP_OK;
}

esp_err_t thermostat_set_config(const thermostat_config_t *config)
{
    if (!config)
        return ESP_ERR_INVALID_ARG;

    g_thermostat_config = *config;
    g_thermostat_config.consigne =
        thermostat_clamp_consigne(g_thermostat_config.consigne);

    thermostat_sync_alerts();
    return thermostat_save_config();
}

const thermostat_runtime_t *thermostat_get_runtime(void)
{
    return &g_thermostat_runtime;
}

thermostat_state_t thermostat_get_state(void)
{
    return g_thermostat_config;
}

void thermostat_set_mode(thermostat_mode_t mode)
{
    g_thermostat_config.mode = mode;
    thermostat_sync_alerts();
    thermostat_save_config();

    ESP_LOGI(TAG, "Mode=%d", mode);
}

void thermostat_set_consigne(float value)
{
    g_thermostat_config.consigne = thermostat_clamp_consigne(value);
    thermostat_sync_alerts();
    thermostat_save_config();

    ESP_LOGI(TAG, "Consigne=%.1f", g_thermostat_config.consigne);
}

void thermostat_set_enabled(bool enabled)
{
    g_thermostat_config.enabled = enabled;
    thermostat_sync_alerts();
    thermostat_save_config();

    ESP_LOGI(TAG, "Enabled=%d", enabled);
}

void thermostat_update_indoor_data(float temp, float hum)
{
    g_thermostat_runtime.temperature = round_float(temp, 2);
    g_thermostat_runtime.humidity = round_float(hum, 2);

    // Met également à jour le contexte global si ton application l'utilise ailleurs
    g_ctx.temperature = g_thermostat_runtime.temperature;
}

void thermostat_update_outdoor_data(float temp, float hum)
{
    g_thermostat_runtime.temp_ext = round_float(temp, 2);
    g_thermostat_runtime.humidity_ext = round_float(hum, 2);
}

void thermostat_update_forecast_data(float temp_1h)
{
    g_thermostat_runtime.temp_forecast_1h = round_float(temp_1h, 2);
}

// ====================================================================
// Génération de la chaîne de statut JSON avec les nouvelles métriques
// ====================================================================

char *thermostat_get_json_status(void)
{

    cJSON *root = cJSON_CreateObject();

    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddBoolToObject(runtime, "initialized", g_thermostat_runtime.initialized);
    cJSON_AddNumberToObject(runtime, "effective_consigne", g_thermostat_runtime.effective_consigne);
    cJSON_AddNumberToObject(runtime, "change_count", g_thermostat_runtime.change_count);
    cJSON_AddStringToObject(runtime, "last_error", g_thermostat_runtime.last_error);
    cJSON_AddNumberToObject(runtime, "temperature", g_thermostat_runtime.temperature);
    cJSON_AddNumberToObject(runtime, "humidity", g_thermostat_runtime.humidity);
    cJSON_AddNumberToObject(runtime, "temp_ext", g_thermostat_runtime.temp_ext);
    cJSON_AddNumberToObject(runtime, "humidity_ext", g_thermostat_runtime.humidity_ext);
    cJSON_AddNumberToObject(runtime, "temp_forecast_1h", g_thermostat_runtime.temp_forecast_1h);
    cJSON_AddBoolToObject(runtime, "state", g_thermostat_runtime.state);

    cJSON *config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddBoolToObject(config, "enabled", g_thermostat_config.enabled);
    cJSON_AddNumberToObject(config, "consigne", g_thermostat_config.consigne);
    cJSON_AddNumberToObject(config, "mode", g_thermostat_config.mode);
    cJSON_AddNumberToObject(config, "hysteresis", g_thermostat_config.hysteresis);
    cJSON_AddNumberToObject(config, "calibration", g_thermostat_config.calibration);
    cJSON_AddBoolToObject(config, "frost_mode", g_thermostat_config.frost_mode);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

void app_init_thermal_model(void)
{
    thermal_model_init(&g_thermal_model);
    thermal_model_load(&g_thermal_model);
    thermal_2r2c_init(1.0f, g_thermostat_runtime.temperature);
}

void app_periodic_update(void)
{
    float Tint = g_thermostat_runtime.temperature;
    float Text = temperature_get_outdoor();
    bool heating_on = get_relay_state();
    int64_t now = time_utils_get_timestamp();

    thermal_model_update(&g_thermal_model, Tint, Text, heating_on, now);

    // Exemple de prédiction à 1h
    float Tint_1h = thermal_model_predict(&g_thermal_model, Tint, Text, heating_on, 3600.0f);
    ESP_LOGI("THERMO", "Pred Tint +1h = %.2f°C", Tint_1h);
}
