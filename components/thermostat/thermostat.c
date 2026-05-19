#include "thermostat.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#include "esp_log.h"
#include "thermostat_storage.h"

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

static void thermostat_update_runtime(void)
{
    g_thermostat_runtime.effective_consigne =
        thermostat_clamp_consigne(g_thermostat_config.consigne);
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
    thermostat_save_config();

    ESP_LOGI(TAG, "Mode=%d", mode);
}

void thermostat_set_consigne(float value)
{
    g_thermostat_config.consigne = thermostat_clamp_consigne(value);
    thermostat_update_runtime();
    thermostat_save_config();

    ESP_LOGI(TAG, "Consigne=%.1f", g_thermostat_config.consigne);
}

void thermostat_set_enabled(bool enabled)
{
    g_thermostat_config.enabled = enabled;
    thermostat_update_runtime();
    thermostat_save_config();

    ESP_LOGI(TAG, "Enabled=%d", enabled);
}

char *thermostat_get_json_status(void)
{
    thermostat_update_runtime();

    cJSON *root = cJSON_CreateObject();

    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddBoolToObject(runtime, "initialized", g_thermostat_runtime.initialized);
    cJSON_AddNumberToObject(runtime, "effective_consigne", g_thermostat_runtime.effective_consigne);
    cJSON_AddNumberToObject(runtime, "change_count", g_thermostat_runtime.change_count);
    cJSON_AddStringToObject(runtime, "last_error", g_thermostat_runtime.last_error);

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
