#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "sd_card.h"
#include "config_storage.h"
#include "task_manager.h"

static const char *TAG = "CONFIG_MGR";

// Chemin des fichiers de configuration
#define PATH_TASK_CONFIG MOUNT_POINT "/tasks.json"

esp_err_t config_storage_save(const char *filename, const char *data, size_t size)
{
    ESP_LOGI(TAG, "Écriture config → %s", filename);

    FILE *f = fopen(filename, "w");
    if (!f) {
        ESP_LOGE(TAG, "Échec ouverture pour écriture : %s", filename);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "Erreur écriture : %u/%u octets", written, size);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t config_storage_load(const char *filename, char **data, size_t *size)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        ESP_LOGW(TAG, "Fichier absent : %s", filename);
        return ESP_ERR_NOT_FOUND;
    }

    FILE *f = fopen(filename, "r");
    if (!f) return ESP_FAIL;

    *data = malloc(st.st_size + 1);
    if (!*data) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read = fread(*data, 1, st.st_size, f);
    fclose(f);

    (*data)[read] = '\0';
    *size = read;

    ESP_LOGI(TAG, "Chargé : %s (%u octets)", filename, read);
    return ESP_OK;
}

cJSON *load_json_from_sdcard(const char *path)
{
    FILE *f = NULL;
    char *data = NULL;
    cJSON *json = NULL;

    ESP_LOGI(TAG, "Chargement du fichier JSON : %s", path);

    /* TRY: ouverture du fichier */
    f = fopen(path, "rb");
    if (!f)
    {
        ESP_LOGW(TAG, "Impossible d'ouvrir %s", path);
        goto error;
    }

    /* TRY: taille du fichier */
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (length <= 0)
    {
        ESP_LOGW(TAG, "Fichier vide ou invalide : %s", path);
        goto error;
    }

    /* TRY: allocation du buffer */
    data = malloc(length + 1);
    if (!data)
    {
        ESP_LOGE(TAG, "Erreur malloc (%ld bytes)", length);
        goto error;
    }

    /* TRY: lecture du fichier */
    size_t read_size = fread(data, 1, length, f);
    if (read_size != length)
    {
        ESP_LOGW(TAG, "Lecture incomplète : %zu/%ld octets", read_size, length);
        goto error;
    }
    data[length] = '\0';

    /* TRY: parsing JSON */
    json = cJSON_Parse(data);
    if (!json)
    {
        const char *err = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "Erreur JSON avant: %s", err ? err : "inconnue");
        goto error;
    }

    /* FINALLY */
    fclose(f);
    free(data);

    ESP_LOGI(TAG, "JSON chargé avec succès.");
    return json;

error:
    /* CATCH: nettoyage + retour NULL */
    if (f) fclose(f);
    if (data) free(data);

    ESP_LOGW(TAG, "Échec du chargement JSON.");
    return NULL;
}

extern task_info_t my_tasks[];   // Ton tableau
extern const int TASK_COUNT;     // Nombre d’entrées

bool save_json_to_sdcard(const char *path)
{
    FILE *f = NULL;
    char *json_str = NULL;
    bool ok = false;

    ESP_LOGI(TAG, "Sauvegarde JSON vers %s", path);

    /* 1) Création de l'objet JSON racine */
    cJSON *root = cJSON_CreateObject();
    if (!root) goto error;

    cJSON *tasks = cJSON_CreateArray();
    if (!tasks) goto error;

    cJSON_AddItemToObject(root, "tasks", tasks);

    /* 2) Remplir le JSON avec les valeurs actuelles */
    for (int i = 0; i < TASK_COUNT; i++)
    {
        cJSON *item = cJSON_CreateObject();
        if (!item) goto error;

        cJSON_AddStringToObject(item, "id", my_tasks[i].key);
        cJSON_AddBoolToObject(item, "enabled",
                              (xEventGroupGetBits(task_manager_get_event_group()) &
                               my_tasks[i].event_bit) != 0);
        cJSON_AddNumberToObject(item, "delay_ms", my_tasks[i].delay_ms);

        cJSON_AddItemToArray(tasks, item);
    }

    /* 3) Convertir en texte JSON formaté */
    json_str = cJSON_Print(root);   // version indentée
    if (!json_str) goto error;

    /* 4) Ouvrir le fichier (création si absent) */
    f = fopen(path, "wb");
    if (!f)
    {
        ESP_LOGE(TAG, "Impossible de créer %s", path);
        goto error;
    }

    /* 5) Écrire le JSON */
    size_t written = fwrite(json_str, 1, strlen(json_str), f);
    fclose(f);
    f = NULL;

    if (written != strlen(json_str))
    {
        ESP_LOGE(TAG, "Erreur d'écriture : %zu/%zu octets",
                 written, strlen(json_str));
        goto error;
    }

    ESP_LOGI(TAG, "JSON sauvegardé avec succès.");
    ok = true;

error:
    if (f) fclose(f);
    if (json_str) free(json_str);
    if (root) cJSON_Delete(root);

    return ok;
}
