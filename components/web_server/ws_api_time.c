#include "ws_api_time.h"
#include "time_utils.h"
#include "time_utils_storage.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include "web_server_metrics.h"

static const char *TAG = "WS_API_TIME";

/* --- HANDLER GET : Lire l'état et la config --- */
static esp_err_t time_api_get_handler(httpd_req_t *req)
{
    char time_str[30];
    time_utils_get_time_str(time_str, sizeof(time_str));

    time_utils_config_t cfg;
    if (!time_utils_storage_load(&cfg))
    {
        strlcpy(cfg.ntp_server, "N/A", sizeof(cfg.ntp_server));
    }

    char json[512];
    snprintf(json, sizeof(json),
             "{\"time\":\"%s\",\"uptime\":%lld,\"last_sync\":%lld,\"ntp_server\":\"%s\",\"max_retry\":%d}",
             time_str,
             (long long)(esp_timer_get_time() / 1000000),
             (long long)time_utils_get_last_sync(),
             cfg.ntp_server,
             cfg.ntp_max_retry);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

/* --- HANDLER POST : Modifier la config --- */
static esp_err_t time_api_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret, remaining = req->content_len;

    // Lecture du corps de la requête
    if (remaining >= sizeof(buf))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0'; // Nul-terminate

    // Ici, on suppose que vous envoyez le nom du serveur brut (ou vous pouvez parser du JSON)
    // Pour simplifier, on traite le corps comme étant le nouveau "ntp_server"
    time_utils_config_t cfg;
    time_utils_storage_load(&cfg);

    strlcpy(cfg.ntp_server, buf, sizeof(cfg.ntp_server));

    if (time_utils_storage_save(&cfg))
    {
        ESP_LOGI(TAG, "Nouveau serveur NTP enregistré : %s", cfg.ntp_server);
        httpd_resp_sendstr(req, "Configuration mise à jour. Redémarrage nécessaire.");
        // Optionnel : esp_restart();
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur NVS");
    }

    return ESP_OK;
}

/* --- ENREGISTREMENT DES URI --- */
esp_err_t ws_register_time_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_TIME: START REGISTER ===");

    g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    esp_err_t err;

    // ---------------- GET TIME ----------------
    httpd_uri_t uri_get = {
        .uri = "/api/time",
        .method = HTTP_GET,
        .handler = time_api_get_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET time)", uri_get.uri);

    err = httpd_register_uri_handler(server, &uri_get);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GET /api/time failed: %s", esp_err_to_name(err));

        g_http_handlers_used += 1;
        // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);
        return err;
    }

    ESP_LOGI(TAG, "GET /api/time -> OK");

    // ---------------- POST TIME ----------------
    httpd_uri_t uri_post = {
        .uri = "/api/time",
        .method = HTTP_POST,
        .handler = time_api_post_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST time)", uri_post.uri);

    err = httpd_register_uri_handler(server, &uri_post);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "POST /api/time failed: %s", esp_err_to_name(err));

        g_http_handlers_used += 1;
        // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);
        return err;
    }

    ESP_LOGI(TAG, "POST /api/time -> OK");

    // ---------------- FINAL ----------------

    g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_TIME: END REGISTER ===");

    ESP_LOGI(TAG, "API Time (GET/POST) prête");
    return ESP_OK;
}
