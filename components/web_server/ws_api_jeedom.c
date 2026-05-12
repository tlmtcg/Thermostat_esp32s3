#include "ws_api_jeedom.h"
#include "jeedom.h"
#include "web_server_metrics.h"
#include "esp_log.h"

static const char *TAG = "WS_API_JEEDOM";

esp_err_t register_jeedom_web_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_JEEDOM: START REGISTER ===");

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    esp_err_t err;

    // ---------------- GET CONFIG ----------------
    static const httpd_uri_t jeedom_get_uri = {
        .uri = "/api/jeedom",
        .method = HTTP_GET,
        .handler = get_jeedom_config_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET)", jeedom_get_uri.uri);

    err = httpd_register_uri_handler(server, &jeedom_get_uri);
    ESP_LOGI(TAG, "Result GET /api/jeedom -> %s", esp_err_to_name(err));

    // ---------------- POST CONFIG ----------------
    static const httpd_uri_t jeedom_post_uri = {
        .uri = "/api/jeedom",
        .method = HTTP_POST,
        .handler = post_jeedom_config_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST)", jeedom_post_uri.uri);

    err = httpd_register_uri_handler(server, &jeedom_post_uri);
    ESP_LOGI(TAG, "Result POST /api/jeedom -> %s", esp_err_to_name(err));

    // ---------------- FINAL ----------------

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_JEEDOM: END REGISTER ===");

    return ESP_OK;
}
