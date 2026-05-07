#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "WS_API_SYS";

static esp_err_t sys_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    // 1. Récupérer le nombre de tâches
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    TaskStatus_t *pxTaskStatusArray = malloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Défaut mémoire");
        return ESP_FAIL;
    }

    // 2. Récupérer les stats détaillées
    uint32_t ulTotalRunTime;
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

    // 3. Construction manuelle du JSON (plus léger que cJSON pour ce cas)
    // On alloue un buffer assez large
    char *json_buf = malloc(uxArraySize * 150 + 200);
    char *ptr = json_buf;

    ptr += sprintf(ptr, "{\"free_heap\":%lu,\"min_heap\":%lu,\"tasks\":[",
                   esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    for (UBaseType_t i = 0; i < uxArraySize; i++)
    {
        // Calcul du CPU % (sécurité contre division par zéro)
        float stats_val = 0;
        if (ulTotalRunTime > 0)
        {
            stats_val = (float)pxTaskStatusArray[i].ulRunTimeCounter / ulTotalRunTime * 100;
        }

        // Conversion du statut en lettre (X, R, B, S)
        char state = '?';
        switch (pxTaskStatusArray[i].eCurrentState)
        {
        case eRunning:
            state = 'X';
            break;
        case eReady:
            state = 'R';
            break;
        case eBlocked:
            state = 'B';
            break;
        case eSuspended:
            state = 'S';
            break;
        case eDeleted:
            state = 'D';
            break;
        default:
            break;
        }

        ptr += sprintf(ptr, "{\"name\":\"%s\",\"state\":\"%c\",\"prio\":%u,\"stack\":%u,\"cpu\":%.1f}%s",
                       pxTaskStatusArray[i].pcTaskName,
                       state,
                       (unsigned int)pxTaskStatusArray[i].uxCurrentPriority,
                       (unsigned int)pxTaskStatusArray[i].usStackHighWaterMark,
                       stats_val,
                       (i == uxArraySize - 1) ? "" : ",");
    }

    sprintf(ptr, "]}");

    httpd_resp_send(req, json_buf, -1);

    // Nettoyage
    free(pxTaskStatusArray);
    free(json_buf);

    return ESP_OK;
}

esp_err_t ws_register_sys_api(httpd_handle_t server)
{
    httpd_uri_t uri_api_sys = {.uri = "/api/sys", .method = HTTP_GET, .handler = sys_status_handler};
    httpd_register_uri_handler(server, &uri_api_sys);
    ESP_LOGI(TAG, "API Système enregistrée");
    return ESP_OK;
}