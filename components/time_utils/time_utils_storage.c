#include "time_utils_storage.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "time_storage";
#define TIME_NAMESPACE "time_cfg"

bool time_utils_storage_load(time_utils_config_t *cfg) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TIME_NAMESPACE, NVS_READONLY, &nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS non trouvé (%s)", esp_err_to_name(err));
        return false;
    }

    size_t len = sizeof(cfg->ntp_server);
    if (nvs_get_str(nvs, "ntp_server", cfg->ntp_server, &len) != ESP_OK) {
        cfg->ntp_server[0] = '\0';
    }

    // Correction type : ntp_max_retry est bien uint8_t
    if (nvs_get_u8(nvs, "ntp_max_retry", &cfg->ntp_max_retry) != ESP_OK) {
        cfg->ntp_max_retry = 10;
    }

    // Correction nom : ntp_sync_interval_sec
    if (nvs_get_u32(nvs, "ntp_sync_int", &cfg->ntp_sync_interval_sec) != ESP_OK) {
        cfg->ntp_sync_interval_sec = 3600;
    }

    nvs_close(nvs);

    ESP_LOGI(TAG, "Config chargée: Server=%s, MaxRetry=%u, Interval=%lu s", 
             cfg->ntp_server, cfg->ntp_max_retry, (unsigned long)cfg->ntp_sync_interval_sec);
             
    return true;
}

bool time_utils_storage_save(const time_utils_config_t *cfg) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TIME_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;

    nvs_set_str(nvs, "ntp_server", cfg->ntp_server);
    nvs_set_u8(nvs, "ntp_max_retry", cfg->ntp_max_retry);
    nvs_set_u32(nvs, "ntp_sync_int", cfg->ntp_sync_interval_sec);
    
    err = nvs_commit(nvs);
    nvs_close(nvs);
    return (err == ESP_OK);
}

bool time_utils_storage_reset_defaults(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TIME_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;

    err = nvs_erase_all(nvs);
    nvs_commit(nvs);
    nvs_close(nvs);
    return (err == ESP_OK);
}
