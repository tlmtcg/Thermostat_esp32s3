#include "ws_api_sht31.h"
#include "esp_log.h"
#include "sht31.h"
#include <stdarg.h>
#include <stdio.h>
#include "i2c_manager.h"

static const char *TAG = "WS_API_SHT31";

static esp_err_t sht31_handler(httpd_req_t *req)
{
    const sht31_state_t *st = sht31_get_state();

    char json[256];

    snprintf(json, sizeof(json),
             "{"
             "\"temperature\":%.2f,"
             "\"humidity\":%.2f,"
             "\"valid\":%s,"
             "\"last_update\":%lld"
             "}",
             st->temperature,
             st->humidity,
             st->valid ? "true" : "false",
             (long long)st->last_update);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t ws_register_sht31_api(httpd_handle_t server)
{
    httpd_uri_t uri = {
        .uri = "/api/sensors/sht31",
        .method = HTTP_GET,
        .handler = sht31_handler};

    httpd_register_uri_handler(server, &uri);

    ESP_LOGI("SHT31_API", "API SHT31 enregistrée");
    return ESP_OK;
}

esp_err_t i2c_manager_reinit(void)
{
    i2c_master_bus_handle_t bus = i2c_manager_get_bus();

    if (!bus) {
        ESP_LOGE(TAG, "Bus I2C non initialisé");
        return ESP_ERR_INVALID_STATE;
    }

    // Rien à faire : le bus existe déjà
    return ESP_OK;
}

