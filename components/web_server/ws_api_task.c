#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "task_manager.h"
#include <string.h>

static const char *TAG = "WS_API_TASK";

/* -------------------------------------------------------------------------- */
/*  HANDLER GET : Récupère l'état de toutes les tâches                        */
/*  Route : GET /api/tasks                                                    */
/* -------------------------------------------------------------------------- */
esp_err_t tasks_get_handler(httpd_req_t *req)
{
    // On définit le type de contenu en JSON
    httpd_resp_set_type(req, "application/json");

    // On récupère l'objet cJSON généré par le task_manager
    cJSON *root = task_manager_get_all_info_json();

    // Conversion de l'objet JSON en chaîne de caractères
    const char *sys_info = cJSON_PrintUnformatted(root);

    // Envoi de la réponse au client Web
    httpd_resp_sendstr(req, sys_info);

    // Libération de la mémoire (Crucial pour éviter les fuites RAM)
    free((void *)sys_info);
    cJSON_Delete(root);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  HANDLER POST : Active ou désactive une tâche spécifique                   */
/*  Route : POST /api/tasks  | Body : {"task": "name", "active": bool}       */
/* -------------------------------------------------------------------------- */
esp_err_t tasks_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            httpd_resp_send_408(req);
        return ESP_FAIL;
    }

    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root)
    {
        cJSON *task = cJSON_GetObjectItem(root, "task");
        cJSON *active = cJSON_GetObjectItem(root, "active");
        cJSON *delay = cJSON_GetObjectItem(root, "delay");

        // On vérifie d'abord que l'identifiant de la tâche est présent
        if (cJSON_IsString(task))
        {
            const char *task_name = task->valuestring;

            // 1. Gestion de l'activation/désactivation (Switch)
            if (cJSON_IsBool(active))
            {
                bool is_active = cJSON_IsTrue(active);
                uint32_t bit = 0;

                if (strcmp(task_name, "weather") == 0)
                    bit = BIT_WEATHER_EN;
                else if (strcmp(task_name, "jeedom") == 0)
                    bit = BIT_JEEDOM_EN;
                else if (strcmp(task_name, "ntp") == 0)
                    bit = BIT_NTP_EN;
                else if (strcmp(task_name, "led") == 0)
                    bit = BIT_LED_EN;
                else if (strcmp(task_name, "storage") == 0)
                    bit = BIT_STORAGE_EN;
                else if (strcmp(task_name, "serial") == 0)
                    bit = BIT_SERIAL_EN;

                if (bit != 0)
                {
                    task_manager_set_active(bit, is_active);
                    ESP_LOGI(TAG, "Tâche '%s' mise à jour : %s", task_name, is_active ? "ON" : "OFF");
                }
            }

            // 2. Gestion du délai (Input Number)
            if (cJSON_IsNumber(delay))
            {
                // Conversion des minutes reçues en millisecondes
                uint32_t new_delay_ms = (uint32_t)(delay->valuedouble * 60 * 1000);

                task_manager_set_delay(task_name, new_delay_ms);
                ESP_LOGI(TAG, "Tâche '%s' -> Nouvel intervalle : %.0f min", task_name, delay->valuedouble);
            }
        }
        cJSON_Delete(root);
    }

    // Réponse standard de succès
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  INITIALISATION : Enregistrement des routes dans le serveur HTTP           */
/* -------------------------------------------------------------------------- */
void ws_register_tasks_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_TASKS: START REGISTER ===");

    esp_err_t err;

    // ---------------- GET TASKS ----------------
    httpd_uri_t get_uri = {
        .uri = "/api/tasks",
        .method = HTTP_GET,
        .handler = tasks_get_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET tasks)", get_uri.uri);

    err = httpd_register_uri_handler(server, &get_uri);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GET /api/tasks failed: %s", esp_err_to_name(err));

        return;
    }

    ESP_LOGI(TAG, "GET /api/tasks -> OK");

    // ---------------- POST TASKS ----------------
    httpd_uri_t post_uri = {
        .uri = "/api/tasks",
        .method = HTTP_POST,
        .handler = tasks_post_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST tasks)", post_uri.uri);

    err = httpd_register_uri_handler(server, &post_uri);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "POST /api/tasks failed: %s", esp_err_to_name(err));

        return;
    }

    ESP_LOGI(TAG, "POST /api/tasks -> OK");

    // ---------------- FINAL ----------------

    // ESP_LOGI(TAG, "=== WS_API_TASKS: END REGISTER ===");

    ESP_LOGI(TAG, "API tasks enregistrée avec succès.");
}
