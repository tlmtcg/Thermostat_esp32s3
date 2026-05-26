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
#include "time_utils.h"
#include "rc_estimator.h"
#include "prediction_engine.h"

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
        float consigne_auto = heating_get_temp_current();
        float Tint = g_thermostat_runtime.temperature;
        float Text = g_thermostat_runtime.temp_ext;

        float tsec = g_thermal_runtime.time_to_reach; // secondes
        int64_t start_ts = g_thermal_runtime.start_heating_at;
        int64_t now = time(NULL);

        // --- 1. Déjà au-dessus de la consigne ? ---
        if (Tint >= consigne_auto)
        {
            // Pas besoin de chauffer
            g_thermostat_runtime.effective_consigne = consigne_auto - 0.2f;
            break;
        }

        // --- 2. Impossible d’atteindre la consigne (chauffage trop faible) ---
        if (tsec < 0)
        {
            // On chauffe quand même, mais sans early-start
            g_thermostat_runtime.effective_consigne = consigne_auto;
            break;
        }

        // --- 3. Early-start : doit-on chauffer maintenant ? ---
        if (start_ts > 0 && now >= start_ts)
        {
            // C’est le moment d’allumer pour atteindre la consigne à l’heure
            g_thermostat_runtime.effective_consigne = consigne_auto;
        }
        else
        {
            // Pas encore l’heure → on laisse descendre légèrement
            g_thermostat_runtime.effective_consigne = consigne_auto - 0.2f;
        }

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

void thermostat_update_indoor_data(float temp, float hum, bool valid_temp)
{
    g_thermostat_runtime.temperature = round_float(temp, 2);
    g_thermostat_runtime.humidity = round_float(hum, 2);
    g_thermostat_runtime.temperature_valid = valid_temp;

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
    cJSON_AddNumberToObject(root, "Ta", g_thermal_runtime.Ta);
    cJSON_AddNumberToObject(root, "Tm", g_thermal_runtime.Tm);

    cJSON_AddNumberToObject(root, "time_to_reach", g_thermal_runtime.time_to_reach);
    cJSON_AddNumberToObject(root, "start_heating_at", g_thermal_runtime.start_heating_at);

    cJSON_AddNumberToObject(root, "Ra", g_thermal_runtime.Ra);
    cJSON_AddNumberToObject(root, "Rm", g_thermal_runtime.Rm);
    cJSON_AddNumberToObject(root, "Ca", g_thermal_runtime.Ca);
    cJSON_AddNumberToObject(root, "Cm", g_thermal_runtime.Cm);
    cJSON_AddNumberToObject(root, "P", g_thermal_runtime.P);

    cJSON_AddNumberToObject(root, "next_consigne", g_thermostat_runtime.next_consigne);    
    cJSON_AddNumberToObject(root, "next_consigne_ts", g_thermostat_runtime.next_consigne_ts);    

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

void app_periodic_update(void)
{
    float Tint = g_thermostat_runtime.temperature;
    float Text = temperature_get_outdoor();
    bool heating_on = get_relay_state();
    int64_t now = time_utils_get_timestamp();

    // Le modèle thermique est déjà mis à jour dans prediction_engine_tick()
    // Ici on ne fait que du monitoring / logging

    thermal_state_t st;
    thermal_2r2c_get_state(&st);

    thermal_params_t prm;
    thermal_2r2c_get_params(&prm);

    ESP_LOGI("THERMO", "2R2C: Ta=%.2f Tm=%.2f", st.Ta, st.Tm);
    ESP_LOGI("THERMO", "Params: Ra=%.3f Rm=%.3f Ca=%.0f Cm=%.0f P=%.0f",
             prm.Ra, prm.Rm, prm.Ca, prm.Cm, prm.P);

    // Exemple : prédiction à 1h via simulation active
    float Tint_1h = thermal_2r2c_simulate_future(3600.0f, Text, heating_on);
    ESP_LOGI("THERMO", "Pred Tint +1h = %.2f°C", Tint_1h);
}

float thermal_2r2c_simulate_future(float horizon_sec, float Text, bool heating)
{
    thermal_state_t st;
    thermal_params_t prm;

    thermal_2r2c_get_state(&st);
    thermal_2r2c_get_params(&prm);

    float Ta = st.Ta;
    float Tm = st.Tm;

    float Ra = prm.Ra;
    float Rm = prm.Rm;
    float Ca = prm.Ca;
    float Cm = prm.Cm;
    float P = prm.P;

    float u = heating ? 1.0f : 0.0f;

    // 🔥 On récupère le dt réel du modèle
    float dt = thermal_2r2c_get_dt();
    int steps = (int)(horizon_sec / dt);

    for (int i = 0; i < steps; i++)
    {
        float dTa = ((Text - Ta) / (Ra * Ca)) + ((Tm - Ta) / (Rm * Ca)) + (u * P / Ca);

        float dTm = ((Ta - Tm) / (Rm * Cm));

        Ta += dTa * dt;
        Tm += dTm * dt;
    }

    return Ta;
}
