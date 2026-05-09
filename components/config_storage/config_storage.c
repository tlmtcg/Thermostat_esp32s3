#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "sd_card.h"
#include "config_storage.h"

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
