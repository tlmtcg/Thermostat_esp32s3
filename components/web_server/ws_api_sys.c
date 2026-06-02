// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_system.h"
// #include "esp_http_server.h"
// #include "esp_log.h"
// #include "esp_heap_caps.h"

// static const char *TAG = "WS_API_SYS";

// static esp_err_t sys_status_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "application/json");

//     // 1. Récupérer le nombre de tâches
//     UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
//     TaskStatus_t *pxTaskStatusArray = malloc(uxArraySize * sizeof(TaskStatus_t));

//     if (pxTaskStatusArray == NULL)
//     {
//         httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Défaut mémoire");
//         return ESP_FAIL;
//     }

//     // 2. Récupérer les stats détaillées
//     uint32_t ulTotalRunTime;
//     uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

//     // 3. Construction manuelle du JSON (plus léger que cJSON pour ce cas)
//     // On alloue un buffer assez large
//     char *json_buf = malloc(uxArraySize * 150 + 200);
//     char *ptr = json_buf;

//     ptr += sprintf(ptr, "{\"free_heap\":%lu,\"min_heap\":%lu,\"tasks\":[",
//                    esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

//     for (UBaseType_t i = 0; i < uxArraySize; i++)
//     {
//         // Calcul du CPU % (sécurité contre division par zéro)
//         float stats_val = 0;
//         if (ulTotalRunTime > 0)
//         {
//             stats_val = (float)pxTaskStatusArray[i].ulRunTimeCounter / ulTotalRunTime * 100;
//         }

//         // Conversion du statut en lettre (X, R, B, S)
//         char state = '?';
//         switch (pxTaskStatusArray[i].eCurrentState)
//         {
//         case eRunning:
//             state = 'X';
//             break;
//         case eReady:
//             state = 'R';
//             break;
//         case eBlocked:
//             state = 'B';
//             break;
//         case eSuspended:
//             state = 'S';
//             break;
//         case eDeleted:
//             state = 'D';
//             break;
//         default:
//             break;
//         }

//         ptr += sprintf(ptr, "{\"name\":\"%s\",\"state\":\"%c\",\"prio\":%u,\"stack\":%u,\"cpu\":%.1f}%s",
//                        pxTaskStatusArray[i].pcTaskName,
//                        state,
//                        (unsigned int)pxTaskStatusArray[i].uxCurrentPriority,
//                        (unsigned int)pxTaskStatusArray[i].usStackHighWaterMark,
//                        stats_val,
//                        (i == uxArraySize - 1) ? "" : ",");
//     }

//     sprintf(ptr, "]}");

//     httpd_resp_send(req, json_buf, -1);

//     // Nettoyage
//     free(pxTaskStatusArray);
//     free(json_buf);

//     return ESP_OK;
// }

// esp_err_t ws_register_sys_api(httpd_handle_t server)
// {
//     // ESP_LOGI(TAG, "=== WS_API_SYS: START REGISTER ===");

//     esp_err_t err;

//     httpd_uri_t uri_api_sys = {
//         .uri = "/api/sys",
//         .method = HTTP_GET,
//         .handler = sys_status_handler,
//         .user_ctx = NULL};

//     ESP_LOGI(TAG, "Register: %s (GET sys)", uri_api_sys.uri);

//     err = httpd_register_uri_handler(server, &uri_api_sys);
//     ESP_LOGI(TAG, "Result /api/sys -> %s", esp_err_to_name(err));

//     // ESP_LOGI(TAG, "=== WS_API_SYS: END REGISTER ===");

//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Sys API registration FAILED");
//         return err;
//     }

//     ESP_LOGI(TAG, "API Système enregistrée avec succès");
//     return ESP_OK;
// }

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "WS_API_SYS";

static esp_err_t sys_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    // 1. Récupérer le nombre de tâches (on ajoute une marge de sécurité pour les tâches créées à la volée)
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks() + 2;
    TaskStatus_t *pxTaskStatusArray = malloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Defaut memoire RAM");
        return ESP_FAIL;
    }

    // 2. Récupérer les stats détaillées
    uint32_t ulTotalRunTime = 0;
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

    // 3. Calcul de la taille requise et allocation résiliente pour le JSON
    size_t json_max_size = (uxArraySize * 150) + 256;
    char *json_buf = heap_caps_malloc(json_max_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!json_buf) 
    {
        json_buf = malloc(json_max_size);
    }

    // CORRECTION CRUCIALE : Libération du premier tableau si le second échoue
    if (json_buf == NULL)
    {
        free(pxTaskStatusArray);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Defaut memoire JSON");
        return ESP_FAIL;
    }

    char *ptr = json_buf;
    size_t remaining_space = json_max_size;
    int written = 0;

    // CORRECTION : Utilisation systématique de snprintf pour éviter les dépassements de buffer
    written = snprintf(ptr, remaining_space, "{\"free_heap\":%lu,\"min_heap\":%lu,\"tasks\":[",
                       (unsigned long)esp_get_free_heap_size(), 
                       (unsigned long)esp_get_minimum_free_heap_size());
    
    if (written > 0 && written < remaining_space) {
        ptr += written;
        remaining_space -= written;
    }

    for (UBaseType_t i = 0; i < uxArraySize; i++)
    {
        // Calcul du CPU % (sécurité division par zéro et support si l'option runtime n'est pas dispo)
        float stats_val = 0.0;
        if (ulTotalRunTime > 0)
        {
            stats_val = (float)pxTaskStatusArray[i].ulRunTimeCounter / ulTotalRunTime * 100.0;
        }

        // Conversion du statut en lettre (X, R, B, S, D)
        char state = '?';
        switch (pxTaskStatusArray[i].eCurrentState)
        {
        case eRunning:   state = 'X'; break;
        case eReady:     state = 'R'; break;
        case eBlocked:   state = 'B'; break;
        case eSuspended: state = 'S'; break;
        case eDeleted:   state = 'D'; break;
        default:                      break;
        }

        written = snprintf(ptr, remaining_space, 
                           "{\"name\":\"%s\",\"state\":\"%c\",\"prio\":%u,\"stack\":%u,\"cpu\":%.1f}%s",
                           pxTaskStatusArray[i].pcTaskName,
                           state,
                           (unsigned int)pxTaskStatusArray[i].uxCurrentPriority,
                           (unsigned int)pxTaskStatusArray[i].usStackHighWaterMark,
                           stats_val,
                           (i == uxArraySize - 1) ? "" : ",");

        if (written > 0 && written < remaining_space) {
            ptr += written;
            remaining_space -= written;
        } else {
            break; // Plus d'espace disponible dans le buffer, on interrompt pour éviter le crash
        }
    }

    snprintf(ptr, remaining_space, "]}");

    // Envoi de la réponse HTTP
    esp_err_t res = httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);

    // Nettoyage centralisé obligatoire
    free(pxTaskStatusArray);
    free(json_buf);

    return res;
}

/* =========================================================
   ENREGISTREMENT API
   ========================================================= */
esp_err_t ws_register_sys_api(httpd_handle_t server)
{
    httpd_uri_t uri_api_sys = {
        .uri = "/api/sys",
        .method = HTTP_GET,
        .handler = sys_status_handler,
        .user_ctx = NULL
    };

    ESP_LOGI(TAG, "Register: %s (GET sys)", uri_api_sys.uri);
    esp_err_t err = httpd_register_uri_handler(server, &uri_api_sys);
    
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Sys API registration FAILED: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "API Systeme enregistree avec succes");
    return ESP_OK;
}
