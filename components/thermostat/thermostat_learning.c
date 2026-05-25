#include "thermostat_learning.h"
#include "thermostat.h"
#include "app_context.h"
#include "weather.h"
#include "esp_log.h"
#include "cJSON.h"
#include <math.h>
#include <time.h>
#include "esp_timer.h"
#include "relay.h"

static const char *TAG = "THERMO_LEARN";

static learning_model_t model = {
    .heat_rate = 0.10f,      // °C/min
    .cool_rate = 0.05f,      // °C/min
    .preferred_evening = 20.5f,
    .preferred_morning = 21.0f,
    .last_temp = NAN,
    .last_update = 0
};

void thermostat_learning_update(float now_temp, float consigne_user)
{
    ESP_LOGD(TAG, "Learning update");
    if (isnan(model.last_temp)) {
        model.last_temp = now_temp;
        model.last_update = esp_timer_get_time() / 1000000;
        return;
    }

    uint32_t now = esp_timer_get_time() / 1000000;
    float dt = (now - model.last_update) / 60.0f; // minutes
    model.last_update = now;

    float dT = now_temp - model.last_temp;
    model.last_temp = now_temp;

    // Apprentissage vitesse de chauffe
    if (dT > 0.05f && get_relay_state()) {
        model.heat_rate = 0.9f * model.heat_rate + 0.1f * (dT / dt);
    }

    // Apprentissage vitesse de refroidissement
    if (dT < -0.05f && !get_relay_state()) {
        model.cool_rate = 0.9f * model.cool_rate + 0.1f * fabsf(dT / dt);
    }

    // Apprentissage des préférences utilisateur
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    if (tm_info->tm_hour >= 18 && tm_info->tm_hour <= 23) {
        model.preferred_evening = 0.95f * model.preferred_evening + 0.05f * consigne_user;
    }

    if (tm_info->tm_hour >= 6 && tm_info->tm_hour <= 9) {
        model.preferred_morning = 0.95f * model.preferred_morning + 0.05f * consigne_user;
    }
}

float thermostat_learning_predict_consigne(void)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    float ext = temperature_get_outdoor();
    if (isnan(ext)) ext = 10.0f;

    // Matin
    if (tm_info->tm_hour >= 6 && tm_info->tm_hour <= 9) {
        return model.preferred_morning + (ext < 5 ? 0.5f : 0.0f);
    }

    // Soir
    if (tm_info->tm_hour >= 18 && tm_info->tm_hour <= 23) {
        return model.preferred_evening + (ext < 5 ? 0.5f : 0.0f);
    }

    // Journée
    return 19.0f + (ext < 5 ? 0.5f : 0.0f);
}

char *thermostat_learning_get_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "heat_rate", model.heat_rate);
    cJSON_AddNumberToObject(root, "cool_rate", model.cool_rate);
    cJSON_AddNumberToObject(root, "preferred_evening", model.preferred_evening);
    cJSON_AddNumberToObject(root, "preferred_morning", model.preferred_morning);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

