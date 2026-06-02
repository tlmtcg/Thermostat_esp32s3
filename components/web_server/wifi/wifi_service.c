// #include "wifi_service.h"
// #include "esp_wifi.h"
// #include "esp_netif.h"
// #include "esp_mac.h"
// #include "wifi_manager.h"
// #include <string.h>
// #include <stdlib.h>

// cJSON *wifi_service_scan(void)
// {
//     wifi_scan_config_t scan_config = {
//         .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true};

//     if (esp_wifi_scan_start(&scan_config, true) != ESP_OK)
//         return NULL;

//     uint16_t ap_count = 0;
//     esp_wifi_scan_get_ap_num(&ap_count);

//     uint16_t number = (ap_count > 15) ? 15 : ap_count;
//     wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * number);
//     if (!records)
//         return NULL;

//     esp_wifi_scan_get_ap_records(&number, records);

//     cJSON *root = cJSON_CreateArray();
//     for (int i = 0; i < number; i++)
//     {
//         cJSON *item = cJSON_CreateObject();
//         cJSON_AddStringToObject(item, "ssid", (char *)records[i].ssid);
//         cJSON_AddNumberToObject(item, "rssi", records[i].rssi);
//         cJSON_AddNumberToObject(item, "chan", records[i].primary);
//         cJSON_AddBoolToObject(item, "auth", records[i].authmode != WIFI_AUTH_OPEN);
//         cJSON_AddItemToArray(root, item);
//     }

//     free(records);
//     return root;
// }

// cJSON *wifi_service_status(void)
// {
//     esp_netif_ip_info_t ip_sta, ip_ap;
//     wifi_ap_record_t ap_info;
//     uint8_t mac[6];

//     esp_read_mac(mac, ESP_MAC_WIFI_STA);

//     esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_sta);
//     esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_ap);

//     char ip_sta_str[16];
//     char ip_ap_str[16];

//     esp_ip4addr_ntoa(&ip_sta.ip, ip_sta_str, sizeof(ip_sta_str));
//     esp_ip4addr_ntoa(&ip_ap.ip, ip_ap_str, sizeof(ip_ap_str));

//     int rssi = -127;
//     int channel = 0;

//     if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
//     {
//         rssi = ap_info.rssi;
//         channel = ap_info.primary;
//     }

//     wifi_sta_list_t stations;
//     esp_wifi_ap_get_sta_list(&stations);

//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "ip_sta", ip_sta_str);
//     cJSON_AddStringToObject(root, "ip_ap", ip_ap_str);
//     cJSON_AddNumberToObject(root, "rssi", rssi);
//     cJSON_AddNumberToObject(root, "chan", channel);
//     cJSON_AddNumberToObject(root, "clients", stations.num);

//     char mac_str[18];
//     snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
//              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

//     cJSON_AddStringToObject(root, "mac", mac_str);

//     return root;
// }

// esp_err_t wifi_service_connect(const char *ssid, const char *pass)
// {
//     wifi_manager_try_connect(ssid, pass);
//     return ESP_OK;
// }



// #include "wifi_service.h"
// #include "esp_wifi.h"
// #include "esp_netif.h"
// #include "esp_mac.h"
// #include "wifi_manager.h"
// #include "esp_log.h"
// #include "cJSON.h"
// #include <string.h>
// #include <stdlib.h>
// #include <stdio.h>

// static const char *TAG = "WIFI_SERVICE";

// /* -------------------------------------------------------------------------- */
// /* SCAN WIFI                                                                  */
// /* -------------------------------------------------------------------------- */
// cJSON *wifi_service_scan(void)
// {
//     wifi_scan_config_t scan_config = {
//         .ssid = NULL, 
//         .bssid = NULL, 
//         .channel = 0, 
//         .show_hidden = true
//     };

//     if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
//         ESP_LOGE(TAG, "Echec esp_wifi_scan_start");
//         return NULL;
//     }

//     uint16_t ap_count = 0;
//     esp_wifi_scan_get_ap_num(&ap_count);

//     uint16_t number = (ap_count > 15) ? 15 : ap_count;
//     if (number == 0) {
//         return cJSON_CreateArray(); // Retourne un tableau vide propre
//     }

//     wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * number);
//     if (!records) {
//         ESP_LOGE(TAG, "Erreur allocation records scan");
//         return NULL;
//     }

//     if (esp_wifi_scan_get_ap_records(&number, records) != ESP_OK) {
//         free(records);
//         return NULL;
//     }

//     cJSON *root = cJSON_CreateArray();
//     if (!root) {
//         free(records);
//         return NULL;
//     }

//     for (int i = 0; i < number; i++)
//     {
//         cJSON *item = cJSON_CreateObject();
//         if (!item) {
//             // CORRECTION CRUCIALE : En cas d'OOM, on nettoie tout l'arbre pour éviter la fuite
//             cJSON_Delete(root);
//             free(records);
//             return NULL;
//         }

//         // CORRECTION : Sécurisation du SSID (garantir la terminaison par \0)
//         char ssid_safe[33];
//         memcpy(ssid_safe, records[i].ssid, sizeof(records[i].ssid));
//         ssid_safe[sizeof(records[i].ssid)] = '\0';

//         cJSON_AddStringToObject(item, "ssid", ssid_safe);
//         cJSON_AddNumberToObject(item, "rssi", records[i].rssi);
//         cJSON_AddNumberToObject(item, "chan", records[i].primary);
//         cJSON_AddBoolToObject(item, "auth", records[i].authmode != WIFI_AUTH_OPEN);
        
//         cJSON_AddItemToArray(root, item);
//     }

//     free(records);
//     return root;
// }

// /* -------------------------------------------------------------------------- */
// /* STATUS WIFI                                                                */
// /* -------------------------------------------------------------------------- */
// cJSON *wifi_service_status(void)
// {
//     esp_netif_ip_info_t ip_sta, ip_ap;
//     wifi_ap_record_t ap_info;
//     uint8_t mac[6];

//     // Initialisation par défaut pour éviter l'envoi de déchets de pile
//     memset(&ip_sta, 0, sizeof(esp_netif_ip_info_t));
//     memset(&ip_ap, 0, sizeof(esp_netif_ip_info_t));

//     esp_read_mac(mac, ESP_MAC_WIFI_STA);

//     // CORRECTION : Vérification des handles netif avant d'interroger les IP (évite Kernel Panic)
//     void *sta_handle = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
//     void *ap_handle  = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

//     if (sta_handle) {
//         esp_netif_get_ip_info(sta_handle, &ip_sta);
//     }
//     if (ap_handle) {
//         esp_netif_get_ip_info(ap_handle, &ip_ap);
//     }

//     char ip_sta_str[16] = "0.0.0.0";
//     char ip_ap_str[16]  = "0.0.0.0";

//     esp_ip4addr_ntoa(&ip_sta.ip, ip_sta_str, sizeof(ip_sta_str));
//     esp_ip4addr_ntoa(&ip_ap.ip, ip_ap_str, sizeof(ip_ap_str));

//     int rssi = -127;
//     int channel = 0;

//     if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
//     {
//         rssi = ap_info.rssi;
//         channel = ap_info.primary;
//     }

//     // Sécurisation de l'appel de la liste des stations connectées (SoftAP)
//     uint32_t client_count = 0;
//     wifi_sta_list_t stations;
//     if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK) {
//         client_count = stations.num;
//     }

//     cJSON *root = cJSON_CreateObject();
//     if (!root) return NULL;

//     cJSON_AddStringToObject(root, "ip_sta", ip_sta_str);
//     cJSON_AddStringToObject(root, "ip_ap", ip_ap_str);
//     cJSON_AddNumberToObject(root, "rssi", rssi);
//     cJSON_AddNumberToObject(root, "chan", channel);
//     cJSON_AddNumberToObject(root, "clients", client_count);

//     char mac_str[18];
//     snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
//              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

//     cJSON_AddStringToObject(root, "mac", mac_str);

//     return root;
// }

// /* -------------------------------------------------------------------------- */
// /* CONNECT WIFI                                                               */
// /* -------------------------------------------------------------------------- */
// esp_err_t wifi_service_connect(const char *ssid, const char *pass)
// {
//     if (!ssid) {
//         return ESP_ERR_INVALID_ARG;
//     }
//     // Appel non bloquant au gestionnaire WiFi global
//     wifi_manager_try_connect(ssid, pass);
//     return ESP_OK;
// }

#include "wifi_service.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "WIFI_SERVICE";

/* -------------------------------------------------------------------------- */
/* SCAN WIFI                                                                  */
/* -------------------------------------------------------------------------- */
cJSON *wifi_service_scan(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL, 
        .bssid = NULL, 
        .channel = 0, 
        .show_hidden = true
    };

    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        ESP_LOGE(TAG, "Echec esp_wifi_scan_start");
        return NULL;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    uint16_t number = (ap_count > 15) ? 15 : ap_count;
    if (number == 0) {
        return cJSON_CreateArray(); 
    }

    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * number);
    if (!records) {
        ESP_LOGE(TAG, "Erreur allocation records scan");
        return NULL;
    }

    cJSON *root = cJSON_CreateArray();
    if (!root) {
        free(records);
        return NULL;
    }

    // CORRECTION : Si la récupération échoue, il faut AUSSI supprimer l'arbre 'root' créé avant
    if (esp_wifi_scan_get_ap_records(&number, records) != ESP_OK) {
        cJSON_Delete(root);
        free(records);
        return NULL;
    }

    for (int i = 0; i < number; i++)
    {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            free(records);
            return NULL;
        }

        // CORRECTION : Formalisation propre et lisible sans risque d'erreur d'indexation
        char ssid_safe[33];
        memset(ssid_safe, 0, sizeof(ssid_safe));
        strncpy(ssid_safe, (char *)records[i].ssid, sizeof(ssid_safe) - 1);

        cJSON_AddStringToObject(item, "ssid", ssid_safe);
        cJSON_AddNumberToObject(item, "rssi", records[i].rssi);
        cJSON_AddNumberToObject(item, "chan", records[i].primary);
        cJSON_AddBoolToObject(item, "auth", records[i].authmode != WIFI_AUTH_OPEN);
        
        cJSON_AddItemToArray(root, item);
    }

    free(records);
    return root;
}

/* -------------------------------------------------------------------------- */
/* STATUS WIFI                                                                */
/* -------------------------------------------------------------------------- */
cJSON *wifi_service_status(void)
{
    esp_netif_ip_info_t ip_sta, ip_ap;
    wifi_ap_record_t ap_info;
    uint8_t mac[6];

    memset(&ip_sta, 0, sizeof(esp_netif_ip_info_t));
    memset(&ip_ap, 0, sizeof(esp_netif_ip_info_t));

    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    void *sta_handle = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    void *ap_handle  = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (sta_handle) {
        esp_netif_get_ip_info(sta_handle, &ip_sta);
    }
    if (ap_handle) {
        esp_netif_get_ip_info(ap_handle, &ip_ap);
    }

    char ip_sta_str[16] = "0.0.0.0";
    char ip_ap_str[16]  = "0.0.0.0";

    esp_ip4addr_ntoa(&ip_sta.ip, ip_sta_str, sizeof(ip_sta_str));
    esp_ip4addr_ntoa(&ip_ap.ip, ip_ap_str, sizeof(ip_ap_str));

    int rssi = -127;
    int channel = 0;

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        rssi = ap_info.rssi;
        channel = ap_info.primary;
    }

    uint32_t client_count = 0;
    wifi_sta_list_t stations;
    if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK) {
        client_count = stations.num;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "ip_sta", ip_sta_str);
    cJSON_AddStringToObject(root, "ip_ap", ip_ap_str);
    cJSON_AddNumberToObject(root, "rssi", rssi);
    cJSON_AddNumberToObject(root, "chan", channel);
    cJSON_AddNumberToObject(root, "clients", client_count);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    cJSON_AddStringToObject(root, "mac", mac_str);

    return root;
}

/* -------------------------------------------------------------------------- */
/* CONNECT WIFI                                                               */
/* -------------------------------------------------------------------------- */
esp_err_t wifi_service_connect(const char *ssid, const char *pass)
{
    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_manager_try_connect(ssid, pass);
    return ESP_OK;
}
