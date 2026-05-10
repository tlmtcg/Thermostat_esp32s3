/**
 * @file led_db.c
 * @brief Implémentation de la base de données pour les configurations LED.
 */

#include "led_db.h"
#include "led_storage.h"
#include "cJSON.h"
#include "esp_log.h"
#include "alert_manager.h"
#include <string.h>

static const char *TAG = "LED_DB";

// Base de données privée
static stored_info_t info_db[10];
static int info_count = 0;
static stored_alarm_t alarm_db[10];
static int alarm_count = 0;

// =============================================================================
// FONCTIONS INTERNES
// =============================================================================

void led_db_add_info(const char *name, led_color_t color)
{
    if (info_count >= 10)
    {
        ESP_LOGW(TAG, "Limite de 10 infos atteinte");
        return;
    }

    strncpy(info_db[info_count].name, name, sizeof(info_db[info_count].name) - 1);
    info_db[info_count].name[sizeof(info_db[info_count].name) - 1] = '\0';
    info_db[info_count].color = color;
    info_count++;
    ESP_LOGI(TAG, "Info ajoutée: %s", name);
    led_db_save();
}

void led_db_add_alarm(const char *name, int blinks, led_color_t color)
{
    if (alarm_count >= 10)
    {
        ESP_LOGW(TAG, "Limite de 10 alarmes atteinte");
        return;
    }

    strncpy(alarm_db[alarm_count].name, name, sizeof(alarm_db[alarm_count].name) - 1);
    alarm_db[alarm_count].name[sizeof(alarm_db[alarm_count].name) - 1] = '\0';
    alarm_db[alarm_count].blinks = blinks;
    alarm_db[alarm_count].color = color;
    alarm_count++;
    ESP_LOGI(TAG, "Alarme ajoutée: %s", name);
    led_db_save();
}

void led_db_delete_by_name(const char *name)
{
    if (!name)
        return;

    // Infos
    for (int i = 0; i < info_count; i++)
    {
        if (strcmp(info_db[i].name, name) == 0)
        {
            for (int j = i; j < info_count - 1; j++)
                info_db[j] = info_db[j + 1];

            info_count--;
            led_db_save();
            ESP_LOGI(TAG, "Info '%s' supprimée", name);
            return;
        }
    }

    // Alarmes
    for (int i = 0; i < alarm_count; i++)
    {
        if (strcmp(alarm_db[i].name, name) == 0)
        {
            for (int j = i; j < alarm_count - 1; j++)
                alarm_db[j] = alarm_db[j + 1];

            alarm_count--;
            led_db_save();
            ESP_LOGI(TAG, "Alarme '%s' supprimée", name);
            return;
        }
    }

    ESP_LOGW(TAG, "Aucun élément trouvé avec le nom '%s'", name);
}

int led_db_get_info_count(void)
{
    return info_count;
}

int led_db_get_alarm_count(void)
{
    return alarm_count;
}

stored_info_t *led_db_get_info_by_idx(int idx)
{
    if (idx >= 0 && idx < info_count)
        return &info_db[idx];
    return NULL;
}

stored_alarm_t *led_db_get_alarm_by_idx(int idx)
{
    if (idx >= 0 && idx < alarm_count)
        return &alarm_db[idx];
    return NULL;
}

void led_db_print_status(void)
{
    ESP_LOGI(TAG, "--- État de la base de données LED ---");
    ESP_LOGI(TAG, "[Infos: %d]", info_count);
    for (int i = 0; i < info_count; i++)
    {
        ESP_LOGI(TAG, "  %d: %s (RGB: %d,%d,%d)",
                 i, info_db[i].name,
                 info_db[i].color.r,
                 info_db[i].color.g,
                 info_db[i].color.b);
    }
    ESP_LOGI(TAG, "[Alarmes: %d]", alarm_count);
    for (int i = 0; i < alarm_count; i++)
    {
        ESP_LOGI(TAG, "  %d: %s (Clignotements: %d, RGB: %d,%d,%d)",
                 i, alarm_db[i].name, alarm_db[i].blinks,
                 alarm_db[i].color.r,
                 alarm_db[i].color.g,
                 alarm_db[i].color.b);
    }
}

void led_db_simulate(int info_idx, int alarm_idx)
{
    // 1. Ambiance
    if (info_idx >= 0 && info_idx < info_count)
    {
        led_set_background(LED_MODE_FIXED, info_db[info_idx].color, 1000);
        ESP_LOGI(TAG, "Simulation ambiance: %s (RGB: %d,%d,%d)",
                 info_db[info_idx].name,
                 info_db[info_idx].color.r,
                 info_db[info_idx].color.g,
                 info_db[info_idx].color.b);
    }

    // 2. Alarme → pipeline A : via alert_manager
    if (alarm_idx >= 0 && alarm_idx < alarm_count)
    {
        const char *name = alarm_db[alarm_idx].name;
        alert_add(name);
        ESP_LOGI(TAG, "Simulation alarme: %s", name);
    }
    else
    {
        // Désactive TOUTES les alarmes actives
        alert_clear_all();
    }
}

// =============================================================================
// INIT / STOCKAGE
// =============================================================================

static char *led_db_serialize(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ESP_LOGE(TAG, "Échec création JSON root");
        return NULL;
    }

    cJSON *infos_arr = cJSON_AddArrayToObject(root, "infos");
    if (!infos_arr)
    {
        ESP_LOGE(TAG, "Échec création tableau 'infos'");
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < info_count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        if (!item)
        {
            ESP_LOGE(TAG, "Échec création objet info");
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddStringToObject(item, "name", info_db[i].name);
        cJSON_AddNumberToObject(item, "r", info_db[i].color.r);
        cJSON_AddNumberToObject(item, "g", info_db[i].color.g);
        cJSON_AddNumberToObject(item, "b", info_db[i].color.b);
        cJSON_AddItemToArray(infos_arr, item);
    }

    cJSON *alarms_arr = cJSON_AddArrayToObject(root, "alarms");
    if (!alarms_arr)
    {
        ESP_LOGE(TAG, "Échec création tableau 'alarms'");
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < alarm_count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        if (!item)
        {
            ESP_LOGE(TAG, "Échec création objet alarm");
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddStringToObject(item, "name", alarm_db[i].name);
        cJSON_AddNumberToObject(item, "blinks", alarm_db[i].blinks);
        cJSON_AddNumberToObject(item, "r", alarm_db[i].color.r);
        cJSON_AddNumberToObject(item, "g", alarm_db[i].color.g);
        cJSON_AddNumberToObject(item, "b", alarm_db[i].color.b);
        cJSON_AddItemToArray(alarms_arr, item);
    }

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

void led_db_save(void)
{
    char *json_data = led_db_serialize();
    if (!json_data)
    {
        ESP_LOGE(TAG, "Échec sérialisation JSON");
        return;
    }

    esp_err_t ret = led_storage_save(json_data, strlen(json_data));
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Échec sauvegarde: %s", esp_err_to_name(ret));

    free(json_data);
}

void led_db_load(void)
{
    char *data = NULL;
    size_t size = 0;

    esp_err_t ret = led_storage_load(&data, &size);
    if (ret == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Aucun fichier de configuration trouvé. Valeurs par défaut.");
        return;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec chargement fichier: %s", esp_err_to_name(ret));
        return;
    }

    cJSON *root = cJSON_ParseWithLength(data, size);
    free(data);

    if (!root)
    {
        ESP_LOGE(TAG, "Échec parsing JSON");
        return;
    }

    info_count = 0;
    alarm_count = 0;

    cJSON *infos = cJSON_GetObjectItem(root, "infos");
    if (infos)
    {
        cJSON *info = NULL;
        cJSON_ArrayForEach(info, infos)
        {
            if (info_count < 10)
            {
                cJSON *name = cJSON_GetObjectItem(info, "name");
                cJSON *r = cJSON_GetObjectItem(info, "r");
                cJSON *g = cJSON_GetObjectItem(info, "g");
                cJSON *b = cJSON_GetObjectItem(info, "b");

                if (name && r && g && b)
                {
                    strncpy(info_db[info_count].name, name->valuestring, sizeof(info_db[info_count].name) - 1);
                    info_db[info_count].name[sizeof(info_db[info_count].name) - 1] = '\0';
                    info_db[info_count].color.r = r->valueint;
                    info_db[info_count].color.g = g->valueint;
                    info_db[info_count].color.b = b->valueint;
                    info_count++;
                }
            }
        }
    }

    cJSON *alarms = cJSON_GetObjectItem(root, "alarms");
    if (alarms)
    {
        cJSON *alarm = NULL;
        cJSON_ArrayForEach(alarm, alarms)
        {
            if (alarm_count < 10)
            {
                cJSON *name = cJSON_GetObjectItem(alarm, "name");
                cJSON *blinks = cJSON_GetObjectItem(alarm, "blinks");
                cJSON *r = cJSON_GetObjectItem(alarm, "r");
                cJSON *g = cJSON_GetObjectItem(alarm, "g");
                cJSON *b = cJSON_GetObjectItem(alarm, "b");

                if (name && blinks && r && g && b)
                {
                    strncpy(alarm_db[alarm_count].name, name->valuestring, sizeof(alarm_db[alarm_count].name) - 1);
                    alarm_db[alarm_count].name[sizeof(alarm_db[alarm_count].name) - 1] = '\0';
                    alarm_db[alarm_count].blinks = blinks->valueint;
                    alarm_db[alarm_count].color.r = r->valueint;
                    alarm_db[alarm_count].color.g = g->valueint;
                    alarm_db[alarm_count].color.b = b->valueint;
                    alarm_count++;
                }
            }
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Configuration chargée: %d infos, %d alarmes", info_count, alarm_count);
}

// =============================================================================
// FONCTIONS PUBLIQUES SUPPLÉMENTAIRES
// =============================================================================

void led_db_init(void)
{
    info_count = 0;
    alarm_count = 0;
    led_db_load();
}

bool led_db_exists(const char *name)
{
    if (name == NULL)
        return false;

    for (int i = 0; i < info_count; i++)
    {
        if (strcmp(info_db[i].name, name) == 0)
            return true;
    }

    for (int i = 0; i < alarm_count; i++)
    {
        if (strcmp(alarm_db[i].name, name) == 0)
            return true;
    }

    return false;
}

int led_db_get_info_idx_by_name(const char *name)
{
    if (name == NULL)
        return -1;

    for (int i = 0; i < info_count; i++)
    {
        if (strcmp(info_db[i].name, name) == 0)
            return i;
    }

    return -1;
}

int led_db_get_alarm_idx_by_name(const char *name)
{
    if (name == NULL)
        return -1;

    for (int i = 0; i < alarm_count; i++)
    {
        if (strcmp(alarm_db[i].name, name) == 0)
            return i;
    }

    return -1;
}
