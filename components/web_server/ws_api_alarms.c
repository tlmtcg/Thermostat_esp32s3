#include "esp_http_server.h"
#include "esp_log.h"
#include "alert_manager.h"
#include "ws_api_alarms.h"
#include "cJSON.h"
#include <stdio.h>
#include "led_ctrl.h"

static const char *TAG = "WS_API_ALARMS";

/* =========================================================
 * GET /api/alarms/active
 * Handler HTTP qui construit un JSON contenant la liste des
 * alarmes actives et l’envoie en réponse
 * ========================================================= */
esp_err_t get_active_alarms_handler(httpd_req_t *req)
{
    ESP_LOGI("WS_API_ALARMS", "get_active_alarms_handler");

    cJSON *root = cJSON_CreateArray();
    if (!root)
        return httpd_resp_send_err(req, 500, "JSON error");

    int count = alert_get_active_count();
    const int *list = alert_get_active_list();

    for (int i = 0; i < count; i++)
    {
        int alarm_idx = list[i];
        stored_alarm_t *alarm = led_db_get_alarm_by_idx(alarm_idx);
        if (!alarm)
            continue;

        cJSON *item = cJSON_CreateObject();

        cJSON_AddStringToObject(item, "name", alarm->name);
        cJSON_AddNumberToObject(item, "blinks", alarm->blinks);
        cJSON_AddNumberToObject(item, "r", alarm->color.r);
        cJSON_AddNumberToObject(item, "g", alarm->color.g);
        cJSON_AddNumberToObject(item, "b", alarm->color.b);

        cJSON_AddItemToArray(root, item);
    }

    char *json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free(json);
    cJSON_Delete(root);

    return ESP_OK;
}

/* =========================================================
 * GET /api/alarms/history
 * Retourne historique complet
 * ========================================================= */
esp_err_t get_history_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateArray();
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
    }

    int added = 0;

    for (int i = 0; i < CONFIG_MAX_ALERT_LOGS; i++)
    {
        const alert_log_t *a = alert_get_by_index(i);
        if (!a || a->timestamp == 0)
            continue;

        cJSON *item = cJSON_CreateObject();
        if (!item)
            continue;

        cJSON_AddStringToObject(item, "name", a->name);
        cJSON_AddBoolToObject(item, "activated", a->activated);
        cJSON_AddNumberToObject(item, "timestamp", (double)a->timestamp);

        cJSON_AddItemToArray(root, item);
        added++;
    }

    char *json = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    if (!json)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free(json);

    return ESP_OK;
}

/* =========================================================
 * POST /api/alarms/clear
 * ========================================================= */
esp_err_t clear_alarms_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "CLEAR alarms");
    alert_clear_all();
    return httpd_resp_sendstr(req, "OK");
}

/* =========================================================
 * DELETE /api/alarms/<timestamp>
 * Exemple: /api/alarms/1777975824
 * ========================================================= */
esp_err_t delete_alarm_handler(httpd_req_t *req)
{
    time_t ts = 0;

    /* =========================================================
     * Parse URL -> timestamp
     * IMPORTANT: time_t = long long sur ESP32
     * ========================================================= */
    if (sscanf(req->uri, "/api/alarms/%lld", (long long *)&ts) != 1)
    {
        ESP_LOGW(TAG, "Bad URI: %s", req->uri);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad URI");
    }

    ESP_LOGI(TAG, "DELETE alarm timestamp: %lld", (long long)ts);

    /* =========================================================
     * Recherche dans l'historique (lecture seule)
     * ========================================================= */
    for (int i = 0;; i++)
    {
        const alert_log_t *a = alert_get_by_index(i);
        if (!a)
            break;  // plus d'entrées

        if (a->timestamp == 0)
            continue;

        if (a->timestamp == ts)
        {
            ESP_LOGI(TAG, "Deleting alarm: %s", a->name);

            // On ne modifie PAS l'historique (a est const),
            // on désactive simplement l'alarme correspondante.
            alert_remove(a->name);

            return httpd_resp_sendstr(req, "OK");
        }
    }

    ESP_LOGW(TAG, "Alarm not found for ts=%lld", (long long)ts);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
}


/* =========================================================
 * REGISTER ROUTES
 * ========================================================= */
esp_err_t ws_register_alarms_api(httpd_handle_t server)
{
    httpd_uri_t uri_active = {
        .uri = "/api/alarms/active",
        .method = HTTP_GET,
        .handler = get_active_alarms_handler};

    httpd_uri_t uri_history = {
        .uri = "/api/alarms/history",
        .method = HTTP_GET,
        .handler = get_history_handler};

    httpd_uri_t uri_clear = {
        .uri = "/api/alarms/clear",
        .method = HTTP_POST,
        .handler = clear_alarms_handler};

    /* IMPORTANT: wildcard route for DELETE */
    httpd_uri_t uri_delete = {
        .uri = "/api/alarms/*",
        .method = HTTP_DELETE,
        .handler = delete_alarm_handler};

    httpd_register_uri_handler(server, &uri_active);
    httpd_register_uri_handler(server, &uri_history);
    httpd_register_uri_handler(server, &uri_clear);
    httpd_register_uri_handler(server, &uri_delete);

    return ESP_OK;
}
