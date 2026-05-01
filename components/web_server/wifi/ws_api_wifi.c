#include "ws_api_wifi.h"
#include "wifi_service.h"
#include "esp_log.h"

static const char *TAG = "WS_API_WIFI";

static esp_err_t wifi_scan_api_handler(httpd_req_t *req)
{
    cJSON *json = wifi_service_scan();
    if (!json)
        return httpd_resp_send_err(req, 500, "Scan failed");

    char *out = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);

    free(out);
    cJSON_Delete(json);
    return ESP_OK;
}

static esp_err_t wifi_api_handler(httpd_req_t *req)
{
    cJSON *json = wifi_service_status();
    char *out = cJSON_PrintUnformatted(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);

    free(out);
    cJSON_Delete(json);
    return ESP_OK;
}

static esp_err_t api_wifi_connect(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    const cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    const cJSON *pass = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(ssid) || !cJSON_IsString(pass))
        return httpd_resp_send_err(req, 400, "Invalid JSON");

    wifi_service_connect(ssid->valuestring, pass->valuestring);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"pending\"}");

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
}
