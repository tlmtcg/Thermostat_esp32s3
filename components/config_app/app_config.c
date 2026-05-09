#include "app_config.h"
#include "config_storage.h"
#include "esp_log.h"

static const char *TAG = "APP_CONFIG";

cJSON* app_config_load_all(void) {
    char *data = NULL;
    size_t size = 0;
    
    if (config_storage_load(APP_CONFIG_PATH, &data, &size) != ESP_OK) {
        return NULL; // Le fichier n'existe pas encore ou SD absente
    }

    cJSON *root = cJSON_Parse(data);
    free(data); // Libère le buffer brut après parsing
    return root;
}

esp_err_t app_config_save_all(cJSON *root) {
    char *out = cJSON_PrintUnformatted(root);
    if (!out) return ESP_ERR_NO_MEM;

    esp_err_t err = config_storage_save(APP_CONFIG_PATH, out, strlen(out));
    free(out);
    return err;
}

cJSON* app_config_get_section(cJSON *root, const char *section_name) {
    if (!root) return NULL;
    cJSON *section = cJSON_GetObjectItem(root, section_name);
    if (!section) {
        section = cJSON_CreateObject();
        cJSON_AddItemToObject(root, section_name, section);
    }
    return section;
}
