#include "ws_api_sht31.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "i2c_manager.h"
#include "sht31.h"

static const char *TAG = "WS_API_SHT31";

static esp_err_t sht31_handler(httpd_req_t *req)
{
    char *json = sht31_get_json_status();
    if (!json)
        return httpd_resp_send_500(req);

    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, json, strlen(json));

    free(json);
    return res;
}

esp_err_t ws_register_sht31_api(httpd_handle_t server)
{
    httpd_uri_t uri = {
        .uri = "/api/sensors/sht31",
        .method = HTTP_GET,
        .handler = sht31_handler,
    };

    esp_err_t err = httpd_register_uri_handler(server, &uri);
    ESP_LOGI(TAG, "API SHT31 enregistree: %s", esp_err_to_name(err));
    return err;
}

esp_err_t i2c_manager_reinit(void)
{
    i2c_master_bus_handle_t bus = i2c_manager_get_bus();

    if (!bus)
    {
        ESP_LOGE(TAG, "Bus I2C non initialise");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
