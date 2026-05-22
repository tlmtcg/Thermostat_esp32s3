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

static const char *TAG = "THERMOSTAT";

static thermostat_config_t g_thermostat_config;

static thermostat_runtime_t g_thermostat_runtime = {
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
    float consigne_auto = heating_get_temp_current();
    g_thermostat_runtime.temperature = round_float(g_ctx.temperature, 2);
    g_thermostat_runtime.state = get_relay_state();

    switch (g_thermostat_config.mode)
    {
    // Mode manuel
    case THERMOSTAT_MODE_MANUAL:
        g_thermostat_runtime.effective_consigne = g_thermostat_config.consigne;
        if (last_mode != g_thermostat_config.mode)
        {
            ESP_LOGI(TAG, "Consigne manu %.2f", g_thermostat_runtime.effective_consigne);
        }
        last_mode = THERMOSTAT_MODE_MANUAL;
        break;

    // Mode automatique horaire
    case THERMOSTAT_MODE_AUTO:
        g_thermostat_runtime.effective_consigne = consigne_auto;
        if (last_mode != g_thermostat_config.mode)
        {
            ESP_LOGI(TAG, "Consigne auto %.2f", g_thermostat_runtime.effective_consigne);
        }
        last_mode = THERMOSTAT_MODE_AUTO;
        break;

    // Mode absent
    case THERMOSTAT_MODE_ABSENT:
        g_thermostat_runtime.effective_consigne = consigne_auto - 4.0f;
        if (last_mode != g_thermostat_config.mode)
        {
            ESP_LOGI(TAG, "Consigne absent %.2f", g_thermostat_runtime.effective_consigne);
        }
        last_mode = THERMOSTAT_MODE_ABSENT;
        break;

        // Mode hors gel
    case THERMOSTAT_MODE_HORS_GEL:
    {
        // 1. PARAMÈTRES CONFIGURABLES
        const float SEUIL_GEL_EXT = 2.0f;
        const float SEUIL_VIGILANCE_INT = 7.0f;
        const float CONSIGNE_MIN_ECO = 5.0f;
        const float CONSIGNE_BOOST_HG = 8.5f;

        // 2. RÉCUPÉRATION DES TEMPÉRATURES
        float ext_temp = temperature_get_outdoor();
        float int_temp = g_thermostat_runtime.temperature;
        bool ext_temp_valide = true;

        // 3. SÉCURISATION : Vérification de la validité du capteur extérieur
        // On vérifie si la valeur est un "Not a Number" (NAN) ou hors limites physiques normales
        if (isnan(ext_temp) || ext_temp < -40.0f || ext_temp > 60.0f)
        {
            ext_temp_valide = false;
            // Mode fail-safe : on simule une température très basse pour forcer la vigilance
            ext_temp = -10.0f;
        }

        // 4. LOGIQUE D'ARBITRAGE INTELLIGENTE
        // Si le capteur extérieur est en panne, ext_temp vaudra -10.0f, la condition dépendra donc uniquement de l'intérieur
        if (ext_temp <= SEUIL_GEL_EXT && int_temp <= SEUIL_VIGILANCE_INT)
        {
            g_thermostat_runtime.effective_consigne = CONSIGNE_BOOST_HG;
        }
        else
        {
            g_thermostat_runtime.effective_consigne = CONSIGNE_MIN_ECO;
        }

        // 5. LOGS ADAPTÉS AVEC ALERTE SÉCURITÉ
        static float last_applied_consigne = -100.0f;
        if (last_mode != g_thermostat_config.mode || g_thermostat_runtime.effective_consigne != last_applied_consigne)
        {
            if (!ext_temp_valide)
            {
                ESP_LOGW(TAG, "ALERTE : Capteur Exterieur INDISPONIBLE ! Securite active. Int: %.1f°C -> Consigne: %.1f°C",
                         int_temp, g_thermostat_runtime.effective_consigne);
            }
            else
            {
                ESP_LOGI(TAG, "HG AFINÉ - Ext: %.1f°C, Int: %.1f°C -> Consigne Appliquée: %.1f°C",
                         ext_temp, int_temp, g_thermostat_runtime.effective_consigne);
            }
            last_applied_consigne = g_thermostat_runtime.effective_consigne;
        }

        last_mode = THERMOSTAT_MODE_HORS_GEL;
        break;
    }

    default:
        g_thermostat_runtime.effective_consigne = g_thermostat_config.consigne;
        break;
    }

    g_thermostat_runtime.effective_consigne = thermostat_clamp_consigne(g_thermostat_runtime.effective_consigne);
}

static void thermostat_update_runtime(void)
{
    thermostat_update_current_consigne();
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
    thermostat_update_runtime();
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

    thermostat_update_runtime();
    thermostat_sync_alerts();
    return thermostat_save_config();
}

const thermostat_runtime_t *thermostat_get_runtime(void)
{
    thermostat_update_runtime();
    return &g_thermostat_runtime;
}

thermostat_state_t thermostat_get_state(void)
{
    return g_thermostat_config;
}

void thermostat_set_mode(thermostat_mode_t mode)
{
    g_thermostat_config.mode = mode;
    thermostat_update_runtime();
    thermostat_sync_alerts();
    thermostat_save_config();

    ESP_LOGI(TAG, "Mode=%d", mode);
}

void thermostat_set_consigne(float value)
{
    g_thermostat_config.consigne = thermostat_clamp_consigne(value);
    thermostat_update_runtime();
    thermostat_sync_alerts();
    thermostat_save_config();

    ESP_LOGI(TAG, "Consigne=%.1f", g_thermostat_config.consigne);
}

void thermostat_set_enabled(bool enabled)
{
    g_thermostat_config.enabled = enabled;
    thermostat_update_runtime();
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
    thermostat_update_runtime();

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