#include <string.h>
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "JEEDOM";

/**
 * Envoie les données du thermostat à Jeedom via ESP-IDF
 * @return true si HTTP 200/201, false sinon.
 */
bool SendStatusJeedom()
{
    // 1. Gardes
    if (!gWifiRuntime.isConnected || !gJeedomConfig.isJeedomEnabled) {
        return false;
    }

    bool success = false;

    // 2. Préparation du JSON avec cJSON (Standard ESP-IDF)
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return false;

    // Remplissage des données simples
    cJSON_AddNumberToObject(root, "temperature",      gSensorRuntime.sensorTemperature);
    cJSON_AddNumberToObject(root, "humidity",         gSensorRuntime.sensorCurrentHumidity);
    cJSON_AddNumberToObject(root, "setpoint",         gConsigneRuntime.effectiveConsigne);
    cJSON_AddNumberToObject(root, "lowCons",          gConsigneConfig.consigneBasse);
    cJSON_AddNumberToObject(root, "hysteresis",       gThermostatConfig.thermostatHysteresisThreshold);
    cJSON_AddNumberToObject(root, "mode",             gThermostatConfig.thermostatCurrentMode);
    cJSON_AddNumberToObject(root, "relay_state",      gHeatingRuntime.heatingNeeded ? 1 : 0);
    cJSON_AddStringToObject(root, "meteoCondition",   gTemperatureRuntime.meteoCondition);
    cJSON_AddNumberToObject(root, "temperature_meteo", gTemperatureRuntime.meteoTemperature);
    cJSON_AddNumberToObject(root, "wifiRssi",         gWifiRuntime.rssi);

    // Gestion de l'Uptime (secondes)
    gJeedomRuntime.uptimeSec = (uint32_t)(esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(root, "uptime", gJeedomRuntime.uptimeSec);
    cJSON_AddStringToObject(root, "apikey", gJeedomConfig.jeedomApiKey);
    cJSON_AddStringToObject(root, "version", APP_VERSION);

    // Récupération et formatage de la MAC Address
    uint8_t mac[6];
    char mac_str[18];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac", mac_str);

    // Ajout du tableau d'alarmes
    cJSON *alarms = cJSON_AddArrayToObject(root, "alarms");
    for (const auto &a : gSystemRuntime.alarmsList) {
        cJSON_AddItemToArray(alarms, cJSON_CreateString(a.alarmName));
    }

    // Sérialisation (équivalent de serializeJson)
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root); // Libère la structure cJSON

    // 3. Configuration de la requête HTTP
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "http://%s:%d%s", 
             gJeedomConfig.jeedomHost, gJeedomConfig.jeedomPort, gJeedomConfig.jeedomPath);

    esp_http_client_config_t config = {};
    config.url = full_url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 5000;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Configuration des headers et du corps de la requête
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, (int)strlen(post_data));

    // 4. Envoi de la requête
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        gJeedomRuntime.lastSendStatus = status_code;

        if (status_code == 200 || status_code == 201) {
            gJeedomRuntime.lastSendTimestamp = (uint32_t)(esp_timer_get_time() / 1000);
            ESP_LOGI(TAG, "✅ Données envoyées avec succès");
            success = true;
        } else {
            ESP_LOGE(TAG, "❌ Erreur HTTP: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "❌ Erreur transport: %s", esp_err_to_name(err));
        gJeedomRuntime.lastSendStatus = -1;
    }

    // 5. Nettoyage final
    esp_http_client_cleanup(client);
    free(post_data); // Libère la chaîne allouée par cJSON_PrintUnformatted

    return success;
}