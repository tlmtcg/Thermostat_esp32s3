#include "ws_api_logs.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include "web_server_metrics.h"

#define LOG_BUFFER_SIZE 4096
static char log_buffer[LOG_BUFFER_SIZE];
static int log_index = 0;

static const char *TAG = "WS_LOGS";

int web_log_vprintf(const char *fmt, va_list args)
{
    char tmp[256];
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);

    if (len > 0)
    {
        for (int i = 0; i < len; i++)
        {
            log_buffer[log_index] = tmp[i];
            log_index = (log_index + 1) % LOG_BUFFER_SIZE;
        }
    }

    return vprintf(fmt, args);
}

static esp_err_t logs_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");

    int idx = log_index;
    for (int i = 0; i < LOG_BUFFER_SIZE; i++)
    {
        char c = log_buffer[(idx + i) % LOG_BUFFER_SIZE];
        httpd_resp_send_chunk(req, &c, 1);
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
    // const char* resp_str = "{\"status\":\"ok\", \"message\":\"Voici les logs\"}";
    // httpd_resp_set_type(req, "application/json");
    // return httpd_resp_send(req, resp_str, strlen(resp_str));
}

esp_err_t ws_register_logs_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_LOGS: START REGISTER ===");

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // Redirection logs vers Web
    esp_log_set_vprintf(web_log_vprintf);

    esp_err_t err;

    httpd_uri_t uri = {
        .uri = "/api/logs",
        .method = HTTP_GET,
        .handler = logs_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET logs)", uri.uri);

    err = httpd_register_uri_handler(server, &uri);

    ESP_LOGI(TAG, "Result /api/logs -> %s", esp_err_to_name(err));

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_LOGS: END REGISTER ===");

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Logs API registration FAILED");
        return err;
    }

    ESP_LOGI(TAG, "API Logs enregistrée avec succès");
    return ESP_OK;
}

void init_web_log_capture(void)
{
    esp_log_set_vprintf(web_log_vprintf);
}
