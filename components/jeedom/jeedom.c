
#include <string.h>
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "weather.h"
#include "sdkconfig.h"
#include "alert_manager.h"
#include "esp_http_server.h"
#include <time.h>
#include "time_utils.h"

static const char *TAG = "JEEDOM";

// Structure pour la configuration en temps réel
typedef struct
{
    char host[64];
    int port;
    char path[128];
    int last_status;
} jeedom_runtime_config_t;

// Initialisation globale avec les valeurs par défaut du menuconfig
jeedom_runtime_config_t gJeedomConfig = {
    .host = CONFIG_JEEDOM_HOST,
    .port = CONFIG_JEEDOM_PORT,
    .path = CONFIG_JEEDOM_PATH,
    .last_status = 0};

// Variable globale pour stocker le dernier JSON envoyé (Debug)
char last_sent_json[512] = "{}";

// Variable pour stocker l'uptime du dernier envoi réussi
uint32_t last_send_time = 0;

char last_send_timestamp[32] = "Jamais";

/**
 * Envoie les données du thermostat à Jeedom via ESP-IDF
 * @return true si HTTP 200/201, false sinon.
 */
bool SendStatusJeedom()
{
// 1. Garde : on vérifie si l'intégration est ACTIVÉE
#ifndef CONFIG_JEEDOM_ENABLE
    ESP_LOGW(TAG, "L'intégration Jeedom est activée.");
    return true;
#endif

    bool success = false;

    // 2. Préparation du JSON
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return false;

    // Remplissage des données
    cJSON_AddNumberToObject(root, "temperature", 20.6);
    cJSON_AddNumberToObject(root, "humidity", 45.2);
    cJSON_AddNumberToObject(root, "setpoint", 19.5);
    cJSON_AddNumberToObject(root, "lowCons", 17.0);
    cJSON_AddNumberToObject(root, "hysteresis", 0.5);
    cJSON_AddNumberToObject(root, "mode", 1);
    cJSON_AddNumberToObject(root, "relay_state", 0);

    // 3. WiFi RSSI
    int8_t rssi = 0;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        rssi = ap_info.rssi;
    }
    cJSON_AddNumberToObject(root, "wifiRssi", rssi);

    // Météo (Assure-toi que latest_weather est accessible ici)
    cJSON_AddStringToObject(root, "meteoCondition", get_weather_description(latest_weather.current.weather_code));
    cJSON_AddNumberToObject(root, "temperature_meteo", latest_weather.current.temperature);

    // Informations système
    cJSON_AddNumberToObject(root, "uptime", (uint32_t)(esp_timer_get_time() / 1000000));
    cJSON_AddStringToObject(root, "apikey", CONFIG_API_KEY);
    cJSON_AddStringToObject(root, "version", "1.0.0"); // Ou esp_get_idf_version()

    // 4. MAC Address
    uint8_t mac[6];
    char mac_str[18];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac", mac_str);

    // 5. Section Alertes
    cJSON *alarms_arr = cJSON_AddArrayToObject(root, "alarms");
    int active_count = alert_get_active_count();
    for (int i = 0; i < active_count; i++)
    {
        const alert_log_t *alert = alert_get_by_index(i);
        if (alert != NULL && alert->activated)
        {
            cJSON_AddItemToArray(alarms_arr, cJSON_CreateString(alert->name));
        }
    }

    // Sérialisation finale
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root); // On libère l'objet cJSON après avoir généré la string

    if (post_data == NULL)
        return false;

    // 6. Configuration de l'URL
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "http://%s:%d%s",
             gJeedomConfig.host,
             gJeedomConfig.port,
             gJeedomConfig.path);

    // On copie le JSON pour l'API de debug avant de libérer post_data
    strncpy(last_sent_json, post_data, sizeof(last_sent_json) - 1);
    last_sent_json[sizeof(last_sent_json) - 1] = '\0';

    ESP_LOGI(TAG, "Envoi vers : %s", full_url);

    esp_http_client_config_t http_config = {
        .url = full_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);

    // Configuration des headers et du corps
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, (int)strlen(post_data));

    // --- 7. Envoi de la requête ---
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);

        if (status_code == 200 || status_code == 201)
        {
            time_utils_get_time_str(last_send_timestamp, sizeof(last_send_timestamp));
            ESP_LOGI(TAG, "✅ Données envoyées avec succès (HTTP %d)", status_code);
            success = true;
        }
        else
        {
            ESP_LOGE(TAG, "❌ Erreur côté serveur Jeedom (HTTP %d)", status_code);
            // Optionnel : afficher la réponse du serveur pour debug
            // int64_t len = esp_http_client_get_content_length(client);
        }
    }
    else
    {
        ESP_LOGE(TAG, "❌ Erreur de transport (Réseau/DNS) : %s", esp_err_to_name(err));
    }

    // --- 8. Nettoyage final ---
    gJeedomConfig.last_status = esp_http_client_get_status_code(client);
    // Libération du client HTTP
    esp_http_client_cleanup(client);

    // Libération de la chaîne de caractères JSON générée par cJSON_Print
    if (post_data != NULL)
    {
        free(post_data);
    }

    // On retourne le résultat de l'opération
    return success;
}

esp_err_t get_jeedom_config_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    // Config et Statut
    cJSON_AddStringToObject(root, "host", gJeedomConfig.host);
    cJSON_AddNumberToObject(root, "port", gJeedomConfig.port);
    cJSON_AddNumberToObject(root, "last_http_code", gJeedomConfig.last_status);

    // AJOUT : Le dernier JSON envoyé
    cJSON_AddStringToObject(root, "last_json", last_sent_json);
    cJSON_AddStringToObject(root, "last_send_time", last_send_timestamp);

    char *json_out = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_out);

    free(json_out);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/jeedom : Modifier la config
esp_err_t post_jeedom_config_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1); // Laisser de la place pour le \0
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return ESP_FAIL;

    cJSON *h = cJSON_GetObjectItem(root, "host");
    if (h && cJSON_IsString(h))
    {
        // Sécurisation de la copie
        strncpy(gJeedomConfig.host, h->valuestring, sizeof(gJeedomConfig.host) - 1);
        gJeedomConfig.host[sizeof(gJeedomConfig.host) - 1] = '\0';
    }

    cJSON *p = cJSON_GetObjectItem(root, "port");
    if (p && cJSON_IsNumber(p))
        gJeedomConfig.port = p->valueint;

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Config mise a jour !");
    return ESP_OK;
}
