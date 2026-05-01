#include "wifi_service.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "wifi_manager.h"
#include <string.h>
#include <stdlib.h>

cJSON *wifi_service_scan(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true};

    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK)
        return NULL;

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    uint16_t number = (ap_count > 15) ? 15 : ap_count;
    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * number);
    if (!records)
        return NULL;

    esp_wifi_scan_get_ap_records(&number, records);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < number; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char *)records[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", records[i].rssi);
        cJSON_AddNumberToObject(item, "chan", records[i].primary);
        cJSON_AddBoolToObject(item, "auth", records[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(root, item);
    }

    free(records);
    return root;
}

cJSON *wifi_service_status(void)
{
    esp_netif_ip_info_t ip_sta, ip_ap;
    wifi_ap_record_t ap_info;
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_sta);
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_ap);

    int rssi = -127;
    int channel = 0;
    const char *auth = "Open";

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        rssi = ap_info.rssi;
        channel = ap_info.primary;
    }

    wifi_sta_list_t stations;
    esp_wifi_ap_get_sta_list(&stations);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ip_sta", ip4addr_ntoa(&ip_sta.ip));
    cJSON_AddStringToObject(root, "ip_ap", ip4addr_ntoa(&ip_ap.ip));
    cJSON_AddNumberToObject(root, "rssi", rssi);
    cJSON_AddNumberToObject(root, "chan", channel);
    cJSON_AddNumberToObject(root, "clients", stations.num);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    cJSON_AddStringToObject(root, "mac", mac_str);

    return root;
}

esp_err_t wifi_service_connect(const char *ssid, const char *pass)
{
    return wifi_manager_try_connect(ssid, pass);
}
