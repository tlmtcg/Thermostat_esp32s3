#include "ws_api_wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include <string.h>
#include <stdlib.h>
#include "esp_mac.h"

static const char *TAG = "WS_API_WIFI";

static esp_err_t wifi_scan_api_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Requête Scan reçue...");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true
    };

    // Lancer le scan (bloquant pour garantir les résultats avant la réponse HTTP)
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur esp_wifi_scan_start: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Scan terminé. Réseaux trouvés : %d", ap_count);

    // On limite à 15 pour éviter de saturer la RAM de l'ESP lors de la création du JSON
    uint16_t number = (ap_count > 15) ? 15 : ap_count;
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * number);
    
    if (!ap_records) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_records));

    // Construction du JSON avec cJSON (plus propre et sécurisé)
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < number; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char *)ap_records[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_records[i].rssi);
        cJSON_AddNumberToObject(item, "chan", ap_records[i].primary);
        cJSON_AddBoolToObject(item, "auth", ap_records[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(root, item);
        
        ESP_LOGD(TAG, "SSID: %s (RSSI: %d)", ap_records[i].ssid, ap_records[i].rssi);
    }

    const char *json_res = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_res, HTTPD_RESP_USE_STRLEN);

    // Libération
    free((void*)json_res);
    cJSON_Delete(root);
    free(ap_records);
    
    return ESP_OK;
}

static esp_err_t wifi_api_handler(httpd_req_t *req)
{
    esp_netif_ip_info_t ip_sta, ip_ap;
    wifi_ap_record_t ap_info;
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_sta);
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_ap);

    int rssi = -127;
    int channel = 0;
    const char *auth_mode = "Ouvert";

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        rssi = ap_info.rssi;
        channel = ap_info.primary;
        switch (ap_info.authmode)
        {
        case WIFI_AUTH_WPA_WPA2_PSK: auth_mode = "WPA/WPA2"; break;
        case WIFI_AUTH_WPA2_PSK:     auth_mode = "WPA2-PSK"; break;
        case WIFI_AUTH_WPA3_PSK:     auth_mode = "WPA3-SAE"; break;
        case WIFI_AUTH_WPA2_WPA3_PSK:auth_mode = "WPA2/WPA3"; break;
        default:                     auth_mode = "WPA2"; break;
        }
    }

    wifi_sta_list_t stations;
    esp_wifi_ap_get_sta_list(&stations);

    char json[512];
    snprintf(json, sizeof(json),
             "{\"ip_sta\":\"" IPSTR "\", \"ip_ap\":\"" IPSTR "\", \"rssi\":%d, "
             "\"auth\":\"%s\", \"mac\":\"%s\", \"chan\":%d, \"clients\":%d}",
             IP2STR(&ip_sta.ip), IP2STR(&ip_ap.ip),
             rssi, auth_mode, mac_str, channel, stations.num);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_wifi_connect(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) {
        return ESP_FAIL;
    }

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    const cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    const cJSON *pass = cJSON_GetObjectItem(root, "password");

    if (cJSON_IsString(ssid) && cJSON_IsString(pass)) {
        ESP_LOGW(TAG, "COMMANDE : Test de connexion vers SSID: %s", ssid->valuestring);
        
        // On lance la tentative via le manager
        wifi_manager_try_connect(ssid->valuestring, pass->valuestring);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"pending\", \"message\":\"Essai lancé\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID/PASS manquants");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t ws_register_wifi_api(httpd_handle_t server)
{
    httpd_uri_t uri_api_wifi  = { .uri = "/api/wifi",        .method = HTTP_GET,  .handler = wifi_api_handler };
    httpd_uri_t uri_api_scan  = { .uri = "/api/wifi/scan",   .method = HTTP_GET,  .handler = wifi_scan_api_handler };
    httpd_uri_t uri_api_conn  = { .uri = "/api/wifi/connect",.method = HTTP_POST, .handler = api_wifi_connect };

    httpd_register_uri_handler(server, &uri_api_wifi);
    httpd_register_uri_handler(server, &uri_api_scan);
    httpd_register_uri_handler(server, &uri_api_conn);

    ESP_LOGI(TAG, "API WiFi enregistrées");
     return ESP_OK;
}
