#include "saison_manager.h"
#include <math.h>
#include <cJSON.h>
#include "esp_log.h"
#include "prediction_engine.h"
#include "sd_card.h"
#include <thermostat.h>

// Historique Text pour moyenne glissante 48h
static float s_text_history[96];
static float s_text_sum = 0.0f;
static int s_text_index = 0;
static bool s_text_filled = false;

static const char *saison_get_profile_filename(void)
{
    switch (g_thermostat_runtime.saison)
    {
    case SAISON_HIVER:
        return "/sdcard/profil_hiver.json";
    case SAISON_ETE:
        return "/sdcard/profil_ete.json";
    default:
        return NULL; // pas de profil pour intersaison
    }
}

// À appeler périodiquement avec la Text courante (dans prediction_engine_tick)
void saison_update_text(float Text_now)
{
    if (s_text_filled)
        s_text_sum -= s_text_history[s_text_index];

    s_text_history[s_text_index] = Text_now;
    s_text_sum += Text_now;

    s_text_index = (s_text_index + 1) % 96;
    if (s_text_index == 0)
        s_text_filled = true;
}

float saison_get_Text_avg_48h(void)
{
    int count = s_text_filled ? 96 : s_text_index;
    if (count == 0)
        return NAN;

    return s_text_sum / (float)count;
}

void saison_update(void)
{
    float avg = saison_get_Text_avg_48h();
    if (isnan(avg))
        return;

    saison_t new_saison;

    if (avg < 12.0f)
        new_saison = SAISON_HIVER;
    else if (avg > 18.0f)
        new_saison = SAISON_ETE;
    else
        new_saison = SAISON_INTERSAISON;

    if (new_saison != g_thermostat_runtime.saison)
    {
        g_thermostat_runtime.saison = new_saison;
        ESP_LOGI("SAISON", "Changement de saison → %d (avg=%.2f)", g_thermostat_runtime.saison, avg);
        saison_load_profile();
    }
}

void saison_save_profile(void)
{
    const char *filename = saison_get_profile_filename();
    if (!filename)
        return;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;

    cJSON_AddNumberToObject(root, "Ra", g_thermal_runtime.Ra);
    cJSON_AddNumberToObject(root, "Rm", g_thermal_runtime.Rm);
    cJSON_AddNumberToObject(root, "Ca", g_thermal_runtime.Ca);
    cJSON_AddNumberToObject(root, "Cm", g_thermal_runtime.Cm);
    cJSON_AddNumberToObject(root, "P", g_thermal_runtime.P);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json)
        return;

    esp_err_t err = sd_write_file(filename, json, "w");
    free(json);

    if (err == ESP_OK)
        ESP_LOGI("SAISON", "Profil sauvegardé dans %s", filename);
    else
        ESP_LOGE("SAISON", "Erreur écriture profil %s", filename);
}

void saison_load_profile(void)
{
    const char *filename = saison_get_profile_filename();
    if (!filename)
        return;

    char *buf = sd_read_file_alloc(filename);
    if (!buf)
        return;

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root)
    {
        ESP_LOGW("SAISON", "JSON invalide dans %s", filename);
        return;
    }

    g_thermal_runtime.Ra = cJSON_GetObjectItem(root, "Ra")->valuedouble;
    g_thermal_runtime.Rm = cJSON_GetObjectItem(root, "Rm")->valuedouble;
    g_thermal_runtime.Ca = cJSON_GetObjectItem(root, "Ca")->valuedouble;
    g_thermal_runtime.Cm = cJSON_GetObjectItem(root, "Cm")->valuedouble;
    g_thermal_runtime.P = cJSON_GetObjectItem(root, "P")->valuedouble;

    // Mettre à jour le modèle sauvegardé
    g_saved_model.Ra = g_thermal_runtime.Ra;
    g_saved_model.Rm = g_thermal_runtime.Rm;
    g_saved_model.Ca = g_thermal_runtime.Ca;
    g_saved_model.Cm = g_thermal_runtime.Cm;
    g_saved_model.P  = g_thermal_runtime.P ;

    cJSON_Delete(root);

    ESP_LOGI("SAISON", "Profil chargé depuis %s", filename);
}
