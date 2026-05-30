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
    cJSON *ts_arr = cJSON_CreateArray();

    // Buffer temporaire pour formater les nombres en chaînes propres
    char num_buf[32];

    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        float temp_val = g_ctx.temp_history[i];
        float hum_val = g_ctx.hum_history[i];

        // 1. Gestion et formatage de la Température
        if (temp_val == 0.0f) {
            cJSON_AddItemToArray(temp_arr, cJSON_CreateNumber(0));
        } else {
            // CORRECTION : On écrit dans le buffer avec exactement 2 décimales
            snprintf(num_buf, sizeof(num_buf), "%.2f", temp_val);
            // On l'ajoute au JSON en tant que Raw (nombre brut sans ré-encodage flottant)
            cJSON_AddItemToArray(temp_arr, cJSON_CreateRaw(num_buf));
        }

        // 2. Gestion et formatage de l'Humidité
        if (hum_val == 0.0f) {
            cJSON_AddItemToArray(hum_arr, cJSON_CreateNumber(0));
        } else {
            // CORRECTION : On écrit dans le buffer avec exactement 1 décimale
            snprintf(num_buf, sizeof(num_buf), "%.1f", hum_val);
            cJSON_AddItemToArray(hum_arr, cJSON_CreateRaw(num_buf));
        }

        // 3. Gestion du Timestamp (Entier, pas de problème de virgule)
        cJSON_AddItemToArray(ts_arr, cJSON_CreateNumber((double)g_ctx.ts_history[i]));
    }

    // Attacher les tableaux à l'objet racine JSON
    cJSON_AddItemToObject(root, "temperature", temp_arr);
    cJSON_AddItemToObject(root, "humidity", hum_arr);
    cJSON_AddItemToObject(root, "timestamp", ts_arr);

    // Génération et envoi du JSON
    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL)
    {
        ESP_LOGE("HTTP", "Echec d'allocation pour le JSON");
        cJSON_Delete(root);
        return httpd_resp_send_500(req);
    }

    httpd_resp_send(req, json, strlen(json));

    // Libération de la mémoire
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
