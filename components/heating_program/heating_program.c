#include "heating_program.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

#include "time_utils.h"

static const char *TAG = "HEATING_PRG";

/* =========================================================
 * CONFIG GLOBALE (UNE SEULE INSTANCE EN RAM)
 * =========================================================
 * ⚠️ IMPORTANT :
 * - static => invisible hors de ce fichier
 * - C'est LA seule source de vérité du thermostat
 */
static chauffage_config_t config;

/* =========================================================
 * SET POINT (PROGRAMMATION)
 * ========================================================= */
esp_err_t heating_set_point(jour_t j,
                            int index,
                            int h,
                            int m,
                            int s,
                            float temp)
{
    if (j >= NB_JOURS || index >= NB_PLAGES)
        return ESP_ERR_INVALID_ARG;

    if (h > 23 || m > 59 || s > 59)
        return ESP_ERR_INVALID_ARG;

    config.planning[j][index].secondes_minuit =
        (uint32_t)(h * 3600 + m * 60 + s);

    config.planning[j][index].temperature = temp;

    return ESP_OK;
}

/* =========================================================
 * TEMPERATURE SELON PLANNING
 * ========================================================= */
float heating_get_temp(
                       jour_t j,
                       uint32_t now_sec)
{
    if (j >= NB_JOURS)
        return -1.0f;

    float temp_cible = config.planning[j][NB_PLAGES - 1].temperature;
    uint32_t dernier_seuil = 0;

    for (int i = 0; i < NB_PLAGES; i++)
    {
        uint32_t seuil = config.planning[j][i].secondes_minuit;

        if (now_sec >= seuil && seuil >= dernier_seuil)
        {
            dernier_seuil = seuil;
            temp_cible = config.planning[j][i].temperature;
        }
    }

    return temp_cible;
}

/* =========================================================
 * INIT NVS + CONFIG
 * ========================================================= */
esp_err_t heating_init(void)
{
    nvs_handle_t h;
    esp_err_t err;

    /* -----------------------------
     * 1. INIT RAM par défaut
     * ----------------------------- */
    memset(&config, 0, sizeof(config));

    for (int d = 0; d < NB_JOURS; d++)
    {
        for (int p = 0; p < NB_PLAGES; p++)
        {
            config.planning[d][p].temperature = 17.0f;
            config.planning[d][p].secondes_minuit = p * 14400; // 4h
        }
    }

    /* -----------------------------
     * 2. OUVERTURE NVS
     * ----------------------------- */
    err = nvs_open("storage", NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open error: %s", esp_err_to_name(err));
        return err;
    }

    /* -----------------------------
     * 3. LECTURE CONFIG
     * ----------------------------- */
    size_t size = sizeof(chauffage_config_t);

    err = nvs_get_blob(h, "heat_v5", &config, &size);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Config chargée (%u bytes)", (unsigned int)size);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "NVS vide -> save defaults");

        nvs_set_blob(h, "heat_v5", &config, sizeof(chauffage_config_t));
        nvs_commit(h);
    }
    else
    {
        ESP_LOGE(TAG, "NVS read error: %s", esp_err_to_name(err));
    }

    nvs_close(h);
    return ESP_OK;
}

/* =========================================================
 * SAVE CONFIG
 * ========================================================= */
esp_err_t heating_save(void)
{
    nvs_handle_t h;
    esp_err_t err;

    // ouverture NVS
    err = nvs_open("storage", NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    // sauvegarde directe de la config globale
    err = nvs_set_blob(h,
                       "heat_v5",
                       &config,
                       sizeof(chauffage_config_t));

    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }

    nvs_close(h);
    return err;
}

/* =========================================================
 * RESET DEFAULTS
 * ========================================================= */
void heating_reset_defaults(void)
{

    float temps[] = {21.0f, 20.0f, 21.0f, 17.0f};
    uint32_t heures[] = {6, 12, 18, 22};

    for (int d = 0; d < NB_JOURS; d++)
    {
        for (int p = 0; p < 4; p++)
        {
            config.planning[d][p].temperature = temps[p];
            config.planning[d][p].secondes_minuit = heures[p] * 3600;
        }
    }
}

/* =========================================================
 * JSON EXPORT
 * ========================================================= */
char *heating_get_json(void)
{

    cJSON *root = cJSON_CreateObject();
    cJSON *days = cJSON_CreateArray();

    for (int d = 0; d < NB_JOURS; d++)
    {
        cJSON *day = cJSON_CreateObject();
        cJSON *slots = cJSON_CreateArray();

        for (int p = 0; p < NB_PLAGES; p++)
        {
            cJSON *slot = cJSON_CreateObject();

            cJSON_AddNumberToObject(slot, "id", p);
            cJSON_AddNumberToObject(slot, "time", config.planning[d][p].secondes_minuit);
            cJSON_AddNumberToObject(slot, "temp", config.planning[d][p].temperature);

            cJSON_AddItemToArray(slots, slot);
        }

        cJSON_AddNumberToObject(day, "day_idx", d);
        cJSON_AddItemToObject(day, "slots", slots);

        cJSON_AddItemToArray(days, day);
    }

    cJSON_AddItemToObject(root, "planning", days);

    char *json = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    return json; // ⚠ free() côté appelant
}

/* =========================================================
 * TEMPERATURE ACTUELLE (LIVE)
 * ========================================================= */
float heating_get_temp_current()
{
    struct tm t = time_utils_get_local_time();

    jour_t j = (jour_t)t.tm_wday-1;

    if (j >= NB_JOURS)
        return -1.0f;

    uint32_t now_sec =
        t.tm_hour * 3600 +
        t.tm_min * 60 +
        t.tm_sec;

    return heating_get_temp( j, now_sec);
}

/* =========================================================
 * ACCES A LA CONFIG GLOBALE (IMPORTANT)
 * ========================================================= */

/* Lecture seule */
const chauffage_config_t *heating_get_config(void)
{
    return &config;
}

/* Lecture + écriture (utile web / UI) */
chauffage_config_t *heating_get_config_rw(void)
{
    return &config;
}

esp_err_t heating_get_program_json(char **out_json)
{
    if (!out_json) return ESP_FAIL;

    *out_json = heating_get_json();
    if (*out_json == NULL) return ESP_FAIL;

    return ESP_OK;
}

esp_err_t heating_reset_program(void)
{
    heating_reset_defaults();
    return heating_save();
}
