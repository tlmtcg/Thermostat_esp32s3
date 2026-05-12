#include "esp_http_server.h"
#include "ws_api_i2c.h"
#include "i2c_manager.h"
#include "config_storage.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include "web_server_metrics.h"

static const char *TAG = "WS_API_I2C";

// Structure pour stocker la configuration I2C
typedef struct
{
    int sda_gpio;
    int scl_gpio;
    int freq_hz;
} i2c_config_t;

// Configuration par défaut (à adapter selon tes besoins)
static i2c_config_t current_config = {
    .sda_gpio = CONFIG_I2C_MANAGER_SDA, // Valeur par défaut depuis sdkconfig
    .scl_gpio = CONFIG_I2C_MANAGER_SCL, // Valeur par défaut depuis sdkconfig
    .freq_hz = CONFIG_I2C_MANAGER_FREQ  // Valeur par défaut depuis sdkconfig
};

/* --- HANDLER POST : Lancer un scan I2C --- */
static esp_err_t i2c_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Requête de scan I2C reçue");

    // Lancer le scan I2C
    esp_err_t ret = i2c_manager_scan();
    if (ret != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Échec du scan I2C");
        return ESP_FAIL;
    }

    // Récupérer le JSON des périphériques
    cJSON *devices_json = i2c_manager_get_devices_json();
    if (devices_json == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Aucun résultat de scan");
        return ESP_FAIL;
    }

    // Convertir le JSON en chaîne
    char *json_str = cJSON_Print(devices_json);
    if (json_str == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Échec de la génération JSON");
        return ESP_FAIL;
    }

    // Envoyer la réponse
    httpd_resp_set_type(req, "application/json");
    esp_err_t resp_ret = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return resp_ret;
}

/* --- HANDLER GET : Lister les périphériques I2C --- */
static esp_err_t i2c_devices_handler(httpd_req_t *req)
{
    cJSON *devices_json = i2c_manager_get_devices_json();
    if (devices_json == NULL)
    {
        // Si aucun scan n'a été effectué, retourner un JSON vide
        const char *empty_json = "{\"status\":\"no_scan_yet\",\"devices\":[]}";
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, empty_json, HTTPD_RESP_USE_STRLEN);
    }

    char *json_str = cJSON_Print(devices_json);
    if (json_str == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Échec de la génération JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t resp_ret = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    return resp_ret;
}

/* --- HANDLER GET : Récupérer la config I2C --- */
static esp_err_t i2c_config_get_handler(httpd_req_t *req)
{
    char json[256];
    snprintf(json, sizeof(json),
             "{\"sda\":%d,\"scl\":%d,\"freq\":%d}",
             current_config.sda_gpio,
             current_config.scl_gpio,
             current_config.freq_hz);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

/* --- HANDLER POST : Modifier la config I2C --- */
static esp_err_t i2c_config_post_handler(httpd_req_t *req)
{
    int total = req->content_len;

    if (total <= 0 || total > 1024)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload invalide");
    }

    char *buf = malloc(total + 1);
    if (!buf)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");

    int received = 0;
    while (received < total)
    {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0)
        {
            free(buf);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Lecture incomplète");
        }
        received += r;
    }

    buf[received] = '\0';
    ESP_LOGI("I2C_API", "POST body = %s", buf);

    // --- Parse JSON ---
    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalide");
    }

    cJSON *sda_json = cJSON_GetObjectItem(root, "sda");
    cJSON *scl_json = cJSON_GetObjectItem(root, "scl");
    cJSON *freq_json = cJSON_GetObjectItem(root, "freq");

    if (sda_json)
        current_config.sda_gpio = sda_json->valueint;
    if (scl_json)
        current_config.scl_gpio = scl_json->valueint;
    if (freq_json)
        current_config.freq_hz = freq_json->valueint;

    cJSON_Delete(root);

    // Sauvegarde + réinit
    i2c_save_config_to_sdcard();
    if (i2c_manager_reinit(current_config.sda_gpio, current_config.scl_gpio, current_config.freq_hz) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Réinit I2C échouée");
    }

    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

// Charge la configuration I2C depuis config.json
static void i2c_load_config_from_sdcard(void)
{
    cJSON *config_json = load_json_from_sdcard(CONFIG_FILE);
    if (config_json == NULL)
    {
        ESP_LOGW(TAG, "Aucune configuration trouvée dans %s, utilisation des valeurs par défaut.", CONFIG_FILE);
        return;
    }

    // Extraire la section "i2c" du JSON
    cJSON *i2c_json = cJSON_GetObjectItem(config_json, "i2c");
    if (i2c_json == NULL)
    {
        ESP_LOGW(TAG, "Aucune section 'i2c' trouvée dans %s.", CONFIG_FILE);
        cJSON_Delete(config_json);
        return;
    }

    // Extraire les valeurs de la section "i2c"
    cJSON *sda_json = cJSON_GetObjectItem(i2c_json, "sda");
    cJSON *scl_json = cJSON_GetObjectItem(i2c_json, "scl");
    cJSON *freq_json = cJSON_GetObjectItem(i2c_json, "freq");

    // Mettre à jour current_config
    if (sda_json != NULL)
        current_config.sda_gpio = sda_json->valueint;
    if (scl_json != NULL)
        current_config.scl_gpio = scl_json->valueint;
    if (freq_json != NULL)
        current_config.freq_hz = freq_json->valueint;

    cJSON_Delete(config_json);
    ESP_LOGI(TAG, "Configuration I2C chargée depuis %s: SDA=%d, SCL=%d, Freq=%d",
             CONFIG_FILE, current_config.sda_gpio, current_config.scl_gpio, current_config.freq_hz);
}

// Sauvegarde la configuration I2C dans config.json
void i2c_save_config_to_sdcard(void)
{
    // Charger le JSON existant (s'il existe)
    cJSON *config_json = load_json_from_sdcard(CONFIG_FILE);
    if (config_json == NULL)
    {
        config_json = cJSON_CreateObject();
        if (config_json == NULL)
        {
            ESP_LOGE(TAG, "Échec de la création du JSON.");
            return;
        }
    }

    // Créer ou mettre à jour la section "i2c"
    cJSON *i2c_json = cJSON_GetObjectItem(config_json, "i2c");
    if (i2c_json == NULL)
    {
        i2c_json = cJSON_AddObjectToObject(config_json, "i2c");
        if (i2c_json == NULL)
        {
            ESP_LOGE(TAG, "Échec de la création de la section 'i2c'.");
            cJSON_Delete(config_json);
            return;
        }
    }

    // Mettre à jour les valeurs dans la section "i2c"
    cJSON_AddNumberToObject(i2c_json, "sda", current_config.sda_gpio);
    cJSON_AddNumberToObject(i2c_json, "scl", current_config.scl_gpio);
    cJSON_AddNumberToObject(i2c_json, "freq", current_config.freq_hz);

    // Sauvegarder le JSON sur la SD card
    char *json_str = cJSON_Print(config_json);
    if (json_str != NULL)
    {
        FILE *file = fopen(CONFIG_FILE, "w");
        if (file != NULL)
        {
            fwrite(json_str, 1, strlen(json_str), file);
            fclose(file);
            ESP_LOGI(TAG, "Configuration I2C sauvegardée dans %s", CONFIG_FILE);
        }
        else
        {
            ESP_LOGE(TAG, "Échec de l'ouverture du fichier %s", CONFIG_FILE);
        }
        free(json_str);
    }
    cJSON_Delete(config_json);
}

/* --- ENREGISTREMENT DES URI --- */
esp_err_t ws_register_i2c_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_I2C: START REGISTER ===");

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // Charger la config SD
    i2c_load_config_from_sdcard();

    esp_err_t err;

    // ---------------- SCAN ----------------
    httpd_uri_t uri_scan = {
        .uri = "/api/i2c/scan",
        .method = HTTP_POST,
        .handler = i2c_scan_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST scan)", uri_scan.uri);

    err = httpd_register_uri_handler(server, &uri_scan);
    ESP_LOGI(TAG, "Result /scan -> %s", esp_err_to_name(err));

    // ---------------- DEVICES ----------------
    httpd_uri_t uri_devices = {
        .uri = "/api/i2c/devices",
        .method = HTTP_GET,
        .handler = i2c_devices_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET devices)", uri_devices.uri);

    err = httpd_register_uri_handler(server, &uri_devices);
    ESP_LOGI(TAG, "Result /devices -> %s", esp_err_to_name(err));

    // ---------------- CONFIG GET ----------------
    httpd_uri_t uri_config_get = {
        .uri = "/api/i2c/config",
        .method = HTTP_GET,
        .handler = i2c_config_get_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET config)", uri_config_get.uri);

    err = httpd_register_uri_handler(server, &uri_config_get);
    ESP_LOGI(TAG, "Result /config GET -> %s", esp_err_to_name(err));

    // ---------------- CONFIG POST ----------------
    httpd_uri_t uri_config_post = {
        .uri = "/api/i2c/config/set",
        .method = HTTP_POST,
        .handler = i2c_config_post_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST config set)", uri_config_post.uri);

    err = httpd_register_uri_handler(server, &uri_config_post);
    ESP_LOGI(TAG, "Result /config POST -> %s", esp_err_to_name(err));

    // ---------------- FINAL ----------------

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_I2C: END REGISTER ===");

    return ESP_OK;
}
