#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "sd_card.h"
#include "config_storage.h"
#include "tasks.h"
#include "config_runtime.h"

static const char *TAG = "CONFIG_MGR";

// Chemin des fichiers de configuration
#define PATH_TASK_CONFIG MOUNT_POINT "/tasks.json"

extern task_info_t my_tasks[];   // Ton tableau
extern const int TASK_COUNT;     // Nombre d’entrées

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

// esp_err_t config_storage_load(const char *filename, char **data, size_t *size)
// {
//     struct stat st;
//     if (stat(filename, &st) != 0) {
//         ESP_LOGW(TAG, "Fichier absent : %s", filename);
//         return ESP_ERR_NOT_FOUND;
//     }

//     FILE *f = fopen(filename, "r");
//     if (!f) return ESP_FAIL;

//     *data = malloc(st.st_size + 1);
//     if (!*data) {
//         fclose(f);
//         return ESP_ERR_NO_MEM;
//     }

//     size_t read = fread(*data, 1, st.st_size, f);
//     fclose(f);

//     (*data)[read] = '\0';
//     *size = read;

//     ESP_LOGI(TAG, "Chargé : %s (%u octets)", filename, read);
//     return ESP_OK;
// }

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

static bool write_cjson_to_file(const char *path, cJSON *root)
{
    if (path == NULL || root == NULL) {
        if (root) cJSON_Delete(root);
        return false;
    }

    // 1. Conversion JSON → chaîne de caractères
    char *json_str = cJSON_Print(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Erreur de serialisation JSON pour %s", path);
        cJSON_Delete(root);
        return false;
    }

    // 2. Sauvegarde physique via votre fonction dédiée
    ESP_LOGI(TAG, "Sauvegarde JSON vers %s (%zu octets)", path, strlen(json_str));
    bool ok = (config_storage_save(path, json_str, strlen(json_str)) == ESP_OK);

    if (!ok) {
        ESP_LOGE(TAG, "Échec de l'écriture du fichier %s", path);
    }

    // 3. Nettoyage unique et centralisé
    free(json_str);
    cJSON_Delete(root);

    return ok;
}

bool save_json_to_sdcard(const char *path)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON *tasks = cJSON_CreateArray();
    if (!tasks) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddItemToObject(root, "tasks", tasks);

    for (int i = 0; i < TASK_COUNT; i++)
    {
        cJSON *item = cJSON_CreateObject();
        if (!item) continue; // On évite le goto, on passe au suivant ou on gère l'erreur

        cJSON_AddStringToObject(item, "id", my_tasks[i].key);
        
        bool is_enabled = (xEventGroupGetBits(tasks_get_event_group()) & my_tasks[i].event_bit) != 0;
        cJSON_AddBoolToObject(item, "enabled", is_enabled);
        cJSON_AddNumberToObject(item, "delay_ms", my_tasks[i].delay_ms);

        cJSON_AddItemToArray(tasks, item);
    }

    // On passe le bébé à la fonction générique
    return write_cjson_to_file(path, root);
}

bool save_kconfig_to_sdcard(const char *path)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    // --- Section I2C ---
    cJSON *i2c = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "i2c", i2c);
    cJSON_AddNumberToObject(i2c, "sda", CONFIG_I2C_MANAGER_SDA);
    cJSON_AddNumberToObject(i2c, "scl", CONFIG_I2C_MANAGER_SCL);
    cJSON_AddNumberToObject(i2c, "freq", CONFIG_I2C_MANAGER_FREQ);

    // --- Section SD ---
    cJSON *sd = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "sdcard", sd);
    cJSON_AddNumberToObject(sd, "mosi", CONFIG_SD_CARD_MOSI_GPIO);
    cJSON_AddNumberToObject(sd, "miso", CONFIG_SD_CARD_MISO_GPIO);
    cJSON_AddNumberToObject(sd, "clk",  CONFIG_SD_CARD_SCLK_GPIO);
    cJSON_AddNumberToObject(sd, "cs",   CONFIG_SD_CARD_CS_GPIO);

    // --- Section TASKS par défaut ---
    cJSON *tasks = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "tasks", tasks);

    for (int i = 0; i < TASK_COUNT; i++)
    {
        cJSON *t = cJSON_CreateObject();
        if (!t) continue;
        cJSON_AddStringToObject(t, "id", my_tasks[i].key);
        cJSON_AddBoolToObject(t, "enabled", true);
        cJSON_AddNumberToObject(t, "delay_ms", my_tasks[i].delay_ms);
        cJSON_AddItemToArray(tasks, t);
    }

    return write_cjson_to_file(path, root);
}

// bool save_nvsconfig_to_sdcard(const char *path, const runtime_config_t *config)
// {
//     if (path == NULL || config == NULL) return false;

//     cJSON *root = cJSON_CreateObject();
//     if (!root) return false;

//     // --- Météo ---
//     cJSON_AddStringToObject(root, "weather_city", config->weather_city);
//     cJSON_AddNumberToObject(root, "weather_lat",  config->weather_lat);
//     cJSON_AddNumberToObject(root, "weather_lon",  config->weather_lon);

//     // --- Thermostat ---
//     cJSON_AddNumberToObject(root, "thermostat_offset",     config->thermostat_offset);
//     cJSON_AddNumberToObject(root, "thermostat_hysteresis", config->thermostat_hysteresis);
//     cJSON_AddBoolToObject(root,   "thermostat_auto_mode",  config->thermostat_auto_mode);

//     // --- Capteurs SHT31 ---
//     cJSON_AddNumberToObject(root, "sht31_temp_calibration", config->sht31_temp_calibration);
//     cJSON_AddNumberToObject(root, "sht31_hum_calibration",  config->sht31_hum_calibration);

//     // --- Jeedom ---
//     cJSON_AddBoolToObject(root,   "jeedom_enabled", config->jeedom_enabled);
//     cJSON_AddNumberToObject(root, "jeedom_id",      (double)config->jeedom_id);

//     // --- WiFi ---
//     cJSON_AddBoolToObject(root,   "wifi_autoreconnect", config->wifi_autoreconnect);

//     return write_cjson_to_file(path, root);
// }

bool save_nvsconfig_to_sdcard(const char *path, const runtime_config_t *config)
{
    if (path == NULL || config == NULL) {
        return false;
    }

    // 1. Création de l'objet racine JSON
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }

    // 2. Ajout des données de la structure au JSON
    
    // --- Météo ---
    cJSON_AddStringToObject(root, "weather_city", config->weather_city);
    cJSON_AddNumberToObject(root, "weather_lat",  config->weather_lat);
    cJSON_AddNumberToObject(root, "weather_lon",  config->weather_lon);

    // --- Thermostat ---
    cJSON_AddNumberToObject(root, "thermostat_offset",     config->thermostat_offset);
    cJSON_AddNumberToObject(root, "thermostat_hysteresis", config->thermostat_hysteresis);
    cJSON_AddBoolToObject(root,   "thermostat_auto_mode",  config->thermostat_auto_mode);

    // --- Capteurs SHT31 ---
    cJSON_AddNumberToObject(root, "sht31_temp_calibration", config->sht31_temp_calibration);
    cJSON_AddNumberToObject(root, "sht31_hum_calibration",  config->sht31_hum_calibration);

    // --- Jeedom ---
    cJSON_AddBoolToObject(root,   "jeedom_enabled", config->jeedom_enabled);
    // Cast en double pour cJSON_AddNumberToObject (qui gère les types numériques)
    cJSON_AddNumberToObject(root, "jeedom_id",      (double)config->jeedom_id);

    // --- WiFi ---
    cJSON_AddBoolToObject(root,   "wifi_autoreconnect", config->wifi_autoreconnect);

    // 3. Génération de la chaîne de caractères JSON (formatée)
    // Utilisez cJSON_PrintUnformatted(root) si vous voulez gagner de l'espace sur la SD
    char *json_str = cJSON_Print(root); 
    if (json_str == NULL) {
        cJSON_Delete(root);
        return false;
    }

    // 4. Sauvegarde sur SD via votre fonction dédiée
    // Note : On utilise strlen(json_str) pour ne pas écrire le '\0' final dans le fichier,
    // ce qui est standard pour du texte JSON.
    bool ok = (config_storage_save(path, json_str, strlen(json_str)) == ESP_OK);

    // 5. Libération de la mémoire
    free(json_str);
    cJSON_Delete(root);

    return ok;
}

bool load_nvsconfig_from_sdcard(const char *path, runtime_config_t *config)
{
    if (path == NULL || config == NULL) return false;

    // 1. Charger et parser le JSON via le moteur refactorisé
    cJSON *root = load_json_from_sdcard(path);
    if (root == NULL) {
        return false;
    }

    // 2. Extraction sécurisée des données
    cJSON *item = NULL;

    // --- Météo ---
    item = cJSON_GetObjectItemCaseSensitive(root, "weather_city");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        strncpy(config->weather_city, item->valuestring, sizeof(config->weather_city) - 1);
        config->weather_city[sizeof(config->weather_city) - 1] = '\0';
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "weather_lat");
    if (cJSON_IsNumber(item)) config->weather_lat = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "weather_lon");
    if (cJSON_IsNumber(item)) config->weather_lon = (float)item->valuedouble;

    // --- Thermostat ---
    item = cJSON_GetObjectItemCaseSensitive(root, "thermostat_offset");
    if (cJSON_IsNumber(item)) config->thermostat_offset = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "thermostat_hysteresis");
    if (cJSON_IsNumber(item)) config->thermostat_hysteresis = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "thermostat_auto_mode");
    if (cJSON_IsBool(item)) config->thermostat_auto_mode = cJSON_IsTrue(item);

    // --- Capteurs SHT31 ---
    item = cJSON_GetObjectItemCaseSensitive(root, "sht31_temp_calibration");
    if (cJSON_IsNumber(item)) config->sht31_temp_calibration = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "sht31_hum_calibration");
    if (cJSON_IsNumber(item)) config->sht31_hum_calibration = (float)item->valuedouble;

    // --- Jeedom ---
    item = cJSON_GetObjectItemCaseSensitive(root, "jeedom_enabled");
    if (cJSON_IsBool(item)) config->jeedom_enabled = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(root, "jeedom_id");
    if (cJSON_IsNumber(item)) config->jeedom_id = (int32_t)item->valuedouble;

    // --- WiFi ---
    item = cJSON_GetObjectItemCaseSensitive(root, "wifi_autoreconnect");
    if (cJSON_IsBool(item)) config->wifi_autoreconnect = cJSON_IsTrue(item);

    // 3. Libération de l'arbre cJSON
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Configuration de l'application chargée avec succès.");
    return true;
}

