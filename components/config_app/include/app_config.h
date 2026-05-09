#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "esp_err.h"
#include "cJSON.h"

#define APP_CONFIG_PATH "/sdcard/app_config.json"

// Charge tout le JSON de la SD en mémoire (cJSON)
cJSON* app_config_load_all(void);

// Sauvegarde l'objet cJSON complet sur la SD
esp_err_t app_config_save_all(cJSON *root);

// Utilitaire : récupère ou crée un objet pour un composant spécifique
cJSON* app_config_get_section(cJSON *root, const char *section_name);

#endif