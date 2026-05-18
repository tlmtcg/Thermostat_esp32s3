#include "thermostat_storage.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "THERMO_STORAGE";

#define NVS_NAMESPACE "thermostat"
#define NVS_KEY_STATE "state"

esp_err_t thermostat_storage_load(thermostat_state_t *state)
{
    if (!state)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;

    esp_err_t err = nvs_open(NVS_NAMESPACE,
                             NVS_READONLY,
                             &handle);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No thermostat config in NVS");

        return err;
    }

    size_t size = sizeof(thermostat_state_t);

    err = nvs_get_blob(handle,
                       NVS_KEY_STATE,
                       state,
                       &size);

    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "nvs_get_blob failed: %s",
                 esp_err_to_name(err));

        return err;
    }

    ESP_LOGI(TAG,
             "Loaded state: mode=%d consigne=%.1f enabled=%d",
             state->mode,
             state->consigne,
             state->enabled);

    return ESP_OK;
}

esp_err_t thermostat_storage_save(const thermostat_state_t *state)
{
    if (!state)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;

    esp_err_t err = nvs_open(NVS_NAMESPACE,
                             NVS_READWRITE,
                             &handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "nvs_open failed: %s",
                 esp_err_to_name(err));

        return err;
    }

    err = nvs_set_blob(handle,
                       NVS_KEY_STATE,
                       state,
                       sizeof(thermostat_state_t));

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "nvs_set_blob failed: %s",
                 esp_err_to_name(err));

        nvs_close(handle);

        return err;
    }

    err = nvs_commit(handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "nvs_commit failed: %s",
                 esp_err_to_name(err));

        nvs_close(handle);

        return err;
    }

    nvs_close(handle);

    ESP_LOGI(TAG,
             "Saved state: mode=%d consigne=%.1f enabled=%d",
             state->mode,
             state->consigne,
             state->enabled);

    return ESP_OK;
}