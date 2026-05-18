#include "thermostat.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "THERMOSTAT";

#define NVS_NAMESPACE "thermostat"
#define NVS_KEY_STATE "state"

static thermostat_state_t state;

/* =========================================================
 * NVS
 * ========================================================= */

static void thermostat_load(void)
{
    nvs_handle_t handle;

    esp_err_t err = nvs_open(NVS_NAMESPACE,
                             NVS_READONLY,
                             &handle);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No saved config");

        state.enabled = true;
        state.consigne = 21.0f;
        state.mode = THERMOSTAT_MODE_HORS_GEL;

        return;
    }

    size_t size = sizeof(state);

    err = nvs_get_blob(handle,
                       NVS_KEY_STATE,
                       &state,
                       &size);

    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Invalid config");

        state.enabled = true;
        state.consigne = 21.0f;
        state.mode = THERMOSTAT_MODE_HORS_GEL;

        return;
    }

    ESP_LOGI(TAG,
             "Loaded: mode=%d consigne=%.1f enabled=%d",
             state.mode,
             state.consigne,
             state.enabled);
}

static void thermostat_save(void)
{
    nvs_handle_t handle;

    esp_err_t err = nvs_open(NVS_NAMESPACE,
                             NVS_READWRITE,
                             &handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }

    err = nvs_set_blob(handle,
                       NVS_KEY_STATE,
                       &state,
                       sizeof(state));

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_set_blob failed");

        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_commit failed");
    }

    nvs_close(handle);

    ESP_LOGI(TAG,
             "Saved: mode=%d consigne=%.1f enabled=%d",
             state.mode,
             state.consigne,
             state.enabled);
}

/* =========================================================
 * PUBLIC
 * ========================================================= */

void thermostat_init(void)
{
    thermostat_load();
}

thermostat_state_t thermostat_get_state(void)
{
    return state;
}

void thermostat_set_mode(thermostat_mode_t mode)
{
    state.mode = mode;
    thermostat_save();
    ESP_LOGI(TAG, "Mode=%d", mode);
}

void thermostat_set_consigne(float value)
{
    if (value < 5.0f)
        value = 5.0f;

    if (value > 35.0f)
        value = 35.0f;

    state.consigne = value;
    thermostat_save();
    ESP_LOGI(TAG, "Consigne=%.1f", value);
}

void thermostat_set_enabled(bool enabled)
{
    state.enabled = enabled;
    thermostat_save();
    ESP_LOGI(TAG, "Enabled=%d", enabled);
}

