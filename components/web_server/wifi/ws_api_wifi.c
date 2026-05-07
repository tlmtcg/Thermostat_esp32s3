#include "ws_api_wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include <string.h>
#include <stdlib.h>
#include "esp_mac.h"
#include "wifi_storage.h"

static const char *TAG = "WS_API_WIFI";

static esp_err_t wifi_config_get_handler(httpd_req_t *req)
{
    ESP_LOGI("WIFI_CFG", "GET /api/wifi/config called");
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "sta_ssid", g_wifi_cfg.sta_ssid);
    cJSON_AddStringToObject(root, "ap_ssid",  g_wifi_cfg.ap_ssid);
    cJSON_AddNumberToObject(root, "ap_channel", g_wifi_cfg.ap_channel);
    cJSON_AddNumberToObject(root, "retry_count", g_wifi_cfg.retry_count);
    cJSON_AddNumberToObject(root, "retry_interval_ms", g_wifi_cfg.retry_interval_ms);
    cJSON_AddNumberToObject(root, "auth_mode", g_wifi_cfg.auth_mode);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return res;
}

static esp_err_t wifi_config_post_handler(httpd_req_t *req)
{
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
        return httpd_resp_send_500(req);

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return httpd_resp_send_500(req);

    // Lecture JSON
    cJSON *sta_ssid  = cJSON_GetObjectItem(root, "sta_ssid");
    cJSON *sta_pass  = cJSON_GetObjectItem(root, "sta_pass");
    cJSON *ap_ssid   = cJSON_GetObjectItem(root, "ap_ssid");
    cJSON *ap_pass   = cJSON_GetObjectItem(root, "ap_pass");
    cJSON *ap_chan   = cJSON_GetObjectItem(root, "ap_channel");
    cJSON *retry_cnt = cJSON_GetObjectItem(root, "retry_count");
    cJSON *retry_int = cJSON_GetObjectItem(root, "retry_interval_ms");
    cJSON *auth_mode = cJSON_GetObjectItem(root, "auth_mode");

    bool ok = true;

    ESP_LOGI(TAG, "===== VALIDATION CONFIG WIFI =====");

    /* ------------------------------
       VALIDATION STA
       ------------------------------ */

    if (cJSON_IsString(sta_ssid) && strlen(sta_ssid->valuestring) > 0)
    {
        strncpy(g_wifi_cfg.sta_ssid, sta_ssid->valuestring, sizeof(g_wifi_cfg.sta_ssid));
        ESP_LOGI(TAG, "STA SSID OK : %s", g_wifi_cfg.sta_ssid);
    }
    else
    {
        ESP_LOGW(TAG, "STA SSID invalide → ignoré");
    }

    if (cJSON_IsString(sta_pass) && strcmp(sta_pass->valuestring, "undefined") != 0)
    {
        strncpy(g_wifi_cfg.sta_pass, sta_pass->valuestring, sizeof(g_wifi_cfg.sta_pass));
        ESP_LOGI(TAG, "STA PASS mis à jour");
    }
    else
    {
        ESP_LOGW(TAG, "STA PASS non fourni → inchangé");
    }

    /* ------------------------------
       VALIDATION AP
       ------------------------------ */

    // SSID AP obligatoire
    if (cJSON_IsString(ap_ssid) && strlen(ap_ssid->valuestring) > 0)
    {
        strncpy(g_wifi_cfg.ap_ssid, ap_ssid->valuestring, sizeof(g_wifi_cfg.ap_ssid));
        ESP_LOGI(TAG, "AP SSID OK : %s", g_wifi_cfg.ap_ssid);
    }
    else
    {
        ESP_LOGE(TAG, "SSID AP invalide → ABANDON SAUVEGARDE");
        ok = false;
    }

    // PASS AP : vide = AP ouvert, sinon >= 8
    if (cJSON_IsString(ap_pass) && strcmp(ap_pass->valuestring, "undefined") != 0)
    {
        size_t lenp = strlen(ap_pass->valuestring);

        if (lenp == 0)
        {
            ESP_LOGW(TAG, "AP PASS vide → AP ouvert");
            g_wifi_cfg.ap_pass[0] = 0;
        }
        else if (lenp >= 8)
        {
            strncpy(g_wifi_cfg.ap_pass, ap_pass->valuestring, sizeof(g_wifi_cfg.ap_pass));
            ESP_LOGI(TAG, "AP PASS OK");
        }
        else
        {
            ESP_LOGE(TAG, "AP PASS trop court (<8) → ABANDON SAUVEGARDE");
            ok = false;
        }
    }
    else
    {
        ESP_LOGW(TAG, "AP PASS non fourni → inchangé");
    }

    // Canal AP
    if (cJSON_IsNumber(ap_chan) && ap_chan->valueint >= 1 && ap_chan->valueint <= 13)
    {
        g_wifi_cfg.ap_channel = ap_chan->valueint;
        ESP_LOGI(TAG, "AP CHAN OK : %d", g_wifi_cfg.ap_channel);
    }
    else
    {
        ESP_LOGE(TAG, "Canal AP invalide → ABANDON SAUVEGARDE");
        ok = false;
    }

    // Retry
    if (cJSON_IsNumber(retry_cnt) && retry_cnt->valueint >= 1 && retry_cnt->valueint <= 20)
        g_wifi_cfg.retry_count = retry_cnt->valueint;

    // Interval
    if (cJSON_IsNumber(retry_int) && retry_int->valueint >= 500)
        g_wifi_cfg.retry_interval_ms = retry_int->valueint;

    // Auth mode
    if (cJSON_IsNumber(auth_mode) && auth_mode->valueint >= 0 && auth_mode->valueint <= 7)
        g_wifi_cfg.auth_mode = auth_mode->valueint;

    cJSON_Delete(root);

    /* ------------------------------
       SAUVEGARDE SI OK
       ------------------------------ */

    if (!ok)
    {
        ESP_LOGE(TAG, "CONFIG NON SAUVEGARDEE (erreurs détectées)");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req,
            "{\"status\":\"error\",\"message\":\"Configuration invalide, rien sauvegardé\"}");
    }

    ESP_LOGI(TAG, "CONFIG VALIDE → SAUVEGARDE NVS");
    wifi_storage_save_all(&g_wifi_cfg);

    ESP_LOGI(TAG, "RELOAD WIFI");
    wifi_manager_reload_config();

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req,
        "{\"status\":\"ok\",\"message\":\"Configuration sauvegardée\"}");
}

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
    httpd_uri_t uri_api_wifi_state  = { .uri = "/api/wifi",         .method = HTTP_GET,  .handler = wifi_api_handler };
    httpd_uri_t uri_api_wifi_scan   = { .uri = "/api/wifi/scan",    .method = HTTP_GET,  .handler = wifi_scan_api_handler };
    httpd_uri_t uri_api_wifi_conn   = { .uri = "/api/wifi/connect", .method = HTTP_POST, .handler = api_wifi_connect };

    httpd_uri_t uri_api_wifi_cfg_get = { .uri = "/api/wifi/config", .method = HTTP_GET,  .handler = wifi_config_get_handler };
    httpd_uri_t uri_api_wifi_cfg_set = { .uri = "/api/wifi",        .method = HTTP_POST, .handler = wifi_config_post_handler };

    httpd_register_uri_handler(server, &uri_api_wifi_state);
    httpd_register_uri_handler(server, &uri_api_wifi_scan);
    httpd_register_uri_handler(server, &uri_api_wifi_conn);
    httpd_register_uri_handler(server, &uri_api_wifi_cfg_get);
    httpd_register_uri_handler(server, &uri_api_wifi_cfg_set);

    ESP_LOGI(TAG, "API WiFi complètes enregistrées");
    return ESP_OK;
}
