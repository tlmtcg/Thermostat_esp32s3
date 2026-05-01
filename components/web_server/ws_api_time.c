#include "ws_api_time.h"
// #include "time_utils.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sntp.h"

extern int64_t last_sntp_sync_time;

static const char *TAG = "WS_API_TIME";

static esp_err_t time_api_handler(httpd_req_t *req)
{
    char time_str[30];
    get_french_time_str(time_str, sizeof(time_str));
    int64_t uptime = esp_timer_get_time() / 1000000;

    char json[256];
    snprintf(json, sizeof(json),
             "{\"time\": \"%s\", \"uptime\": %lld, \"last_sync\": %lld}",
             time_str, (long long)uptime, (long long)last_sntp_sync_time);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t ws_register_time_api(httpd_handle_t server)
{
    httpd_uri_t uri_api_time = { .uri = "/api/time", .method = HTTP_GET, .handler = time_api_handler };
    httpd_register_uri_handler(server, &uri_api_time);
    ESP_LOGI(TAG, "API Time enregistrée");
    return ESP_OK;
}
