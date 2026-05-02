/**
 * @file led_storage.c
 * @brief Implémentation du stockage pour les configurations LED.
 */

#include "led_storage.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "LED_STORAGE";
// Il faut que le fichier partitions.csv existe
#define LED_CONFIG_FILE "/spiffs/led_config.json"

esp_err_t led_storage_init(void) {
    ESP_LOGI(TAG, "Initialisation de SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage", // Doit exister dans partitions.csv
        .max_files = 5,
        .format_if_mount_failed = true // Formate si la partition est vide/corrompue
    };

    ESP_LOGI(TAG, "Tentative de montage sur partition '%s'...", conf.partition_label);

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Échec du montage ou formatage du système de fichiers");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partition '%s' introuvable dans la table des partitions", conf.partition_label);
        } else {
            ESP_LOGE(TAG, "Erreur SPIFFS: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec de la récupération des infos SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS monté. Taille totale: %d, Utilisé: %d", total, used);
    return ESP_OK;
}

esp_err_t led_storage_save(const char *data, size_t size) {
    FILE *f = fopen(LED_CONFIG_FILE, "w");
    if (!f) {
        ESP_LOGE(TAG, "Impossible d'ouvrir %s pour l'écriture", LED_CONFIG_FILE);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "Erreur lors de l'écriture (écrit: %d/%d)", written, size);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Données sauvegardées (%d octets)", written);
    return ESP_OK;
}

esp_err_t led_storage_load(char **data, size_t *size) {
    FILE *f = fopen(LED_CONFIG_FILE, "r");
    if (!f) {
        ESP_LOGW(TAG, "Fichier %s introuvable", LED_CONFIG_FILE);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    *data = malloc(fsize + 1);
    if (!*data) {
        fclose(f);
        ESP_LOGE(TAG, "Mémoire insuffisante pour charger le fichier");
        return ESP_ERR_NO_MEM;
    }

    size_t read = fread(*data, 1, fsize, f);
    fclose(f);

    if (read != fsize) {
        free(*data);
        ESP_LOGE(TAG, "Erreur de lecture du fichier");
        return ESP_FAIL;
    }

    (*data)[fsize] = '\0';
    *size = fsize;
    return ESP_OK;
}

