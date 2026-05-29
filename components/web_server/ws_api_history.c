#include "ws_api_history.h"
#include "app_context.h" // <-- contient app_context_t et g_ctx
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "WS_HISTORY";

static esp_err_t history_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");

    cJSON *temp_arr = cJSON_CreateArray();
    cJSON *hum_arr = cJSON_CreateArray();

    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        cJSON_AddItemToArray(temp_arr,
                             cJSON_CreateNumber(g_ctx.temp_history[i]));

        cJSON_AddItemToArray(hum_arr,
                             cJSON_CreateNumber(g_ctx.hum_history[i]));
    }

    cJSON_AddItemToObject(root, "temperature", temp_arr);
    cJSON_AddItemToObject(root, "humidity", hum_arr);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t ws_register_history_api(httpd_handle_t server)
{
    httpd_uri_t uri = {
        .uri = "/api/history",
        .method = HTTP_GET,
        .handler = history_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s", uri.uri);
    return httpd_register_uri_handler(server, &uri);
}
