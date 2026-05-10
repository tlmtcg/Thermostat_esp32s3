#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include "esp_err.h"

// Charge le contenu d'un fichier depuis la SD
esp_err_t config_storage_load(const char *filename, char **data, size_t *size);

// Sauvegarde du texte (JSON) dans un fichier sur la SD
esp_err_t config_storage_save(const char *filename, const char *data, size_t size);

cJSON *load_json_from_sdcard(const char *path);

bool save_json_to_sdcard(const char *path);
#endif
