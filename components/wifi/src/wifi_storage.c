#include "wifi_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WIFI_STORAGE";

bool wifi_storage_load(char *ssid, size_t ssid_len,
                       char *pass, size_t pass_len)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_store", NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "nvs_open READONLY: %s", esp_err_to_name(err));
        return false;
    }

    size_t s_len = ssid_len;
    size_t p_len = pass_len;
    esp_err_t res_s = nvs_get_str(nvs, "ssid", ssid, &s_len);
    esp_err_t res_p = nvs_get_str(nvs, "pass", pass, &p_len);
    nvs_close(nvs);

    if (res_s == ESP_OK)
        ESP_LOGI(TAG, "SSID NVS: %s", ssid);
    if (res_p != ESP_OK)
        ESP_LOGW(TAG, "PASS non trouvé en NVS");

    return (res_s == ESP_OK);
}

bool wifi_storage_save(const char *ssid, const char *pass)
{
    if (!ssid || strlen(ssid) == 0)
    {
        ESP_LOGE(TAG, "SSID vide, pas de sauvegarde");
        return false;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_store", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open WRITE: %s", esp_err_to_name(err));
        return false;
    }

    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", pass ? pass : "");
    err = nvs_commit(nvs);
    nvs_close(nvs);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Credentials sauvegardés");
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "nvs_commit: %s", esp_err_to_name(err));
        return false;
    }
}
