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
esp_err_t tasks_get_handler(httpd_req_t *req) {
    // On définit le type de contenu en JSON
    httpd_resp_set_type(req, "application/json");
    
    // On récupère l'objet cJSON généré par le task_manager
    cJSON *root = task_manager_get_all_info_json();
    
    // Conversion de l'objet JSON en chaîne de caractères
    const char *sys_info = cJSON_PrintUnformatted(root);
    
    // Envoi de la réponse au client Web
    httpd_resp_sendstr(req, sys_info);
    
    // Libération de la mémoire (Crucial pour éviter les fuites RAM)
    free((void*)sys_info);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  HANDLER POST : Active ou désactive une tâche spécifique                   */
/*  Route : POST /api/tasks  | Body : {"task": "name", "active": bool}       */
/* -------------------------------------------------------------------------- */
esp_err_t tasks_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    
    buf[ret] = '\0'; // Assure la fin de chaîne pour le parseur JSON

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *task = cJSON_GetObjectItem(root, "task");
        cJSON *active = cJSON_GetObjectItem(root, "active");

        // Vérification du format du JSON reçu
        if (cJSON_IsString(task) && cJSON_IsBool(active)) {
            bool is_active = cJSON_IsTrue(active);
            const char* task_name = task->valuestring;

            // Mapping entre la clé Web et les bits FreeRTOS
            if (strcmp(task_name, "weather") == 0) {
                task_manager_set_active(BIT_WEATHER_EN, is_active);
            } 
            else if (strcmp(task_name, "jeedom") == 0) {
                task_manager_set_active(BIT_JEEDOM_EN, is_active);
            } 
            else if (strcmp(task_name, "ntp") == 0) {
                task_manager_set_active(BIT_NTP_EN, is_active);
            }
            
            ESP_LOGI(TAG, "Tâche '%s' mise à jour : %s", task_name, is_active ? "ON" : "OFF");
        }
        cJSON_Delete(root);
    }

    // Réponse standard de succès
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  INITIALISATION : Enregistrement des routes dans le serveur HTTP           */
/* -------------------------------------------------------------------------- */
void ws_register_tasks_api(httpd_handle_t server) {
    // Configuration de la route GET
    httpd_uri_t get_uri = {
        .uri       = "/api/tasks",
        .method    = HTTP_GET,
        .handler   = tasks_get_handler,
        .user_ctx  = NULL
    };

    // Configuration de la route POST
    httpd_uri_t post_uri = {
        .uri       = "/api/tasks",
        .method    = HTTP_POST,
        .handler   = tasks_post_handler,
        .user_ctx  = NULL
    };

    // Enregistrement effectif auprès du serveur
    httpd_register_uri_handler(server, &get_uri);
    httpd_register_uri_handler(server, &post_uri);

    ESP_LOGI(TAG, "API de gestion des tâches enregistrée avec succès.");
}
