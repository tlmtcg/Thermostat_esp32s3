#include "thermal_model.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <math.h>

static const char *TAG = "THERMAL_MODEL";

thermal_model_t g_thermal_model;

#define NVS_NAMESPACE      "thermal_model"
#define NVS_KEY_R          "R"
#define NVS_KEY_C          "C"
#define NVS_KEY_P          "P"

// Valeurs par défaut raisonnables pour une maison moyenne
#define DEFAULT_R          0.15f   // K/W
#define DEFAULT_C          5e6f    // J/K
#define DEFAULT_P          8000.0f // W (chaudière / radiateurs)

void thermal_model_init(thermal_model_t *model)
{
    if (!model) return;

    model->R = DEFAULT_R;
    model->C = DEFAULT_C;
    model->P = DEFAULT_P;

    model->last_Tint = NAN;
    model->last_Text = NAN;
    model->last_ts   = 0;
    model->initialized = false;
}

esp_err_t thermal_model_load(thermal_model_t *model)
{
    if (!model) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open RO failed (%s), using defaults", esp_err_to_name(err));
        thermal_model_init(model);
        return ESP_OK;
    }

    size_t len = sizeof(float);
    float R, C, P;

    err = nvs_get_blob(handle, NVS_KEY_R, &R, &len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No R in NVS, using defaults");
        thermal_model_init(model);
        nvs_close(handle);
        return ESP_OK;
    }

    len = sizeof(float);
    err = nvs_get_blob(handle, NVS_KEY_C, &C, &len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No C in NVS, using defaults");
        thermal_model_init(model);
        nvs_close(handle);
        return ESP_OK;
    }

    len = sizeof(float);
    err = nvs_get_blob(handle, NVS_KEY_P, &P, &len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No P in NVS, using defaults");
        thermal_model_init(model);
        nvs_close(handle);
        return ESP_OK;
    }

    model->R = R;
    model->C = C;
    model->P = P;

    model->last_Tint = NAN;
    model->last_Text = NAN;
    model->last_ts   = 0;
    model->initialized = false;

    ESP_LOGI(TAG, "Loaded model: R=%.4f, C=%.1f, P=%.1f", model->R, model->C, model->P);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t thermal_model_save(const thermal_model_t *model)
{
    if (!model) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open RW failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_R, &model->R, sizeof(float));
    if (err != ESP_OK) goto end;

    err = nvs_set_blob(handle, NVS_KEY_C, &model->C, sizeof(float));
    if (err != ESP_OK) goto end;

    err = nvs_set_blob(handle, NVS_KEY_P, &model->P, sizeof(float));
    if (err != ESP_OK) goto end;

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved model: R=%.4f, C=%.1f, P=%.1f", model->R, model->C, model->P);
    }

end:
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving model: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}

/**
 * Mise à jour simple du modèle :
 *  - on observe dT/dt
 *  - si chauffage ON : on ajuste P/C
 *  - si chauffage OFF : on ajuste 1/(R*C)
 *
 * On utilise un filtrage exponentiel pour lisser les valeurs.
 */
void thermal_model_update(thermal_model_t *model,
                          float Tint,
                          float Text,
                          bool heating_on,
                          int64_t timestamp_us)
{
    if (!model) return;

    if (!model->initialized) {
        model->last_Tint = Tint;
        model->last_Text = Text;
        model->last_ts   = timestamp_us;
        model->initialized = true;
        return;
    }

    float dt = (timestamp_us - model->last_ts) / 1e6f; // en secondes
    if (dt <= 10.0f) {
        // Trop peu de temps écoulé, on ignore
        return;
    }

    float dT = Tint - model->last_Tint;
    float dTdt = dT / dt;

    // On évite les valeurs aberrantes
    if (fabsf(dTdt) > 0.05f) { // > 0.05 °C/s = 3°C/min, trop violent
        model->last_Tint = Tint;
        model->last_Text = Text;
        model->last_ts   = timestamp_us;
        return;
    }

    // Filtrage exponentiel
    const float alpha = 0.02f; // apprentissage lent

    if (heating_on) {
        // Chauffage ON : dT/dt ≈ (Text - Tint)/R/C + P/C
        // On approxime P/C à partir de la pente observée
        float term_env = (Text - Tint) / (model->R * model->C);
        float P_over_C = dTdt - term_env;
        if (P_over_C > 0.0f) {
            float new_P = P_over_C * model->C;
            model->P = (1.0f - alpha) * model->P + alpha * new_P;
        }
    } else {
        // Chauffage OFF : dT/dt ≈ (Text - Tint)/R/C
        // => R*C ≈ (Text - Tint) / dTdt
        if (fabsf(dTdt) > 1e-5f) {
            float RC = (Text - Tint) / dTdt;
            if (RC > 0.0f) {
                float new_C = RC / model->R;
                // On borne C pour éviter les dérives
                if (new_C > 1e5f && new_C < 5e7f) {
                    model->C = (1.0f - alpha) * model->C + alpha * new_C;
                }
            }
        }
    }

    model->last_Tint = Tint;
    model->last_Text = Text;
    model->last_ts   = timestamp_us;
}

/**
 * Intégration simple dT/dt sur dt_seconds, en supposant Text constant
 * et état du chauffage constant.
 */
float thermal_model_predict(const thermal_model_t *model,
                            float Tint_now,
                            float Text_now,
                            bool heating_on,
                            float dt_seconds)
{
    if (!model || dt_seconds <= 0.0f) return Tint_now;

    float P = heating_on ? model->P : 0.0f;

    // dT/dt = (Text - Tint)/(R*C) + P/C
    // On linéarise en supposant Tint ~ Tint_now sur l’intervalle
    float dTdt = (Text_now - Tint_now) / (model->R * model->C) + P / model->C;

    float Tint_future = Tint_now + dTdt * dt_seconds;

    // On borne un peu pour éviter les délires
    if (Tint_future < -10.0f) Tint_future = -10.0f;
    if (Tint_future > 40.0f)  Tint_future = 40.0f;

    return Tint_future;
}
