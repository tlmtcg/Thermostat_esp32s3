#include "led_storage.h"
#include "esp_log.h"
#include "sd_card.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "LED_STORAGE";
#define LED_CONFIG_FILE  MOUNT_POINT "/led_config.json"

esp_err_t led_storage_init(void)
{
    // Rien à faire : la SD est déjà montée par init_sd_card()
    ESP_LOGI(TAG, "LED storage initialisé (SD)");
    return ESP_OK;
}

esp_err_t led_storage_save(const char *data, size_t size)
{
    ESP_LOGI(TAG, "Sauvegarde LED → %s", LED_CONFIG_FILE);

    FILE *f = fopen(LED_CONFIG_FILE, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "Impossible d'ouvrir %s", LED_CONFIG_FILE);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size)
    {
        ESP_LOGE(TAG, "Erreur écriture LED (%d/%d)", written, size);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sauvegarde LED OK (%d octets)", written);
    return ESP_OK;
}

esp_err_t led_storage_load(char **data, size_t *size)
{
    ESP_LOGI(TAG, "Chargement LED depuis %s", LED_CONFIG_FILE);

    FILE *f = fopen(LED_CONFIG_FILE, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "Aucun fichier LED trouvé");
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    *data = malloc(fsize + 1);
    if (!*data)
    {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(*data, 1, fsize, f);
    (*data)[fsize] = '\0';
    *size = fsize;

    fclose(f);

    ESP_LOGI(TAG, "Chargement LED OK (%ld octets)", fsize);
    return ESP_OK;
}
