#include <esp_http_server.h>
#include "ws_api_led.h"
#include "cJSON.h"
#include "led_ctrl.h"  // Décommenté pour utiliser led_color_t et les prototypes
#include "esp_log.h"
#include <string.h> 

static const char *TAG = "WS_API_LED";

// --- Vérifier si un nom existe déjà ---
bool name_exists(const char *name)
{
    for (int i = 0; i < led_db_get_info_count(); i++)
    {
        stored_info_t *item = led_db_get_info_by_idx(i);
        if (item && strcmp(item->name, name) == 0) return true;
    }
    for (int i = 0; i < led_db_get_alarm_count(); i++)
    {
        stored_alarm_t *item = led_db_get_alarm_by_idx(i);
        if (item && strcmp(item->name, name) == 0) return true;
    }
    return false;
}

// --- 1. GET /api/led : Renvoie l'état de la mémoire ---
static esp_err_t led_status_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    // Section INFOS
    cJSON *infos_array = cJSON_AddArrayToObject(root, "infos");
    for (int i = 0; i < led_db_get_info_count(); i++)
    {
        stored_info_t *item = led_db_get_info_by_idx(i);
        if (item)
        {
            cJSON *info = cJSON_CreateObject();
            cJSON_AddNumberToObject(info, "id", i);
            cJSON_AddStringToObject(info, "name", item->name);
            cJSON_AddNumberToObject(info, "r", item->color.r);
            cJSON_AddNumberToObject(info, "g", item->color.g);
            cJSON_AddNumberToObject(info, "b", item->color.b);
            cJSON_AddItemToArray(infos_array, info);
        }
    }

    // Section ALARMES
    cJSON *alarms_array = cJSON_AddArrayToObject(root, "alarms");
    for (int i = 0; i < led_db_get_alarm_count(); i++)
    {
        stored_alarm_t *item = led_db_get_alarm_by_idx(i);
        if (item)
        {
            cJSON *alarm = cJSON_CreateObject();
            cJSON_AddNumberToObject(alarm, "id", i);
            cJSON_AddStringToObject(alarm, "name", item->name);
            cJSON_AddNumberToObject(alarm, "blinks", item->blinks);
            cJSON_AddNumberToObject(alarm, "r", item->color.r);
            cJSON_AddNumberToObject(alarm, "g", item->color.g);
            cJSON_AddNumberToObject(alarm, "b", item->color.b);
            cJSON_AddItemToArray(alarms_array, alarm);
        }
    }

    const char *sys_json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, sys_json);

    cJSON_Delete(root);
    free((void *)sys_json);
    return ESP_OK;
}

// --- 2. POST /api/led/simulate ---
static esp_err_t led_simulate_post_handler(httpd_req_t *req)
{
    char buf[150];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalide");
        return ESP_FAIL;
    }

    cJSON *info_idx_item = cJSON_GetObjectItem(root, "info_idx");
    cJSON *alarm_idx_item = cJSON_GetObjectItem(root, "alarm_idx");

    if (info_idx_item || alarm_idx_item)
    {
        int info_idx = info_idx_item ? info_idx_item->valueint : -1;
        int alarm_idx = alarm_idx_item ? alarm_idx_item->valueint : -1;

        ESP_LOGI(TAG, "Web Simulate -> Info: %d, Alarme: %d", info_idx, alarm_idx);
        led_db_simulate(info_idx, alarm_idx);
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Aucun index fourni");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// --- 3. POST /api/led/off ---
static esp_err_t led_off_post_handler(httpd_req_t *req)
{
    led_stop(); // Utilisation de la fonction propre définie dans led_ctrl.h
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"off\"}");
    return ESP_OK;
}

// --- 4. POST /api/led/add ---
static esp_err_t led_add_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    cJSON *name_item = cJSON_GetObjectItem(root, "name");
    cJSON *color_item = cJSON_GetObjectItem(root, "color");
    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    cJSON *blinks_item = cJSON_GetObjectItem(root, "blinks");

    if (!name_item || !color_item || !type_item || !name_item->valuestring)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Données incomplètes");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (name_exists(name_item->valuestring))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Le nom existe déjà");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Préparation de la couleur
    int r, g, b;
    sscanf(color_item->valuestring, "#%02x%02x%02x", &r, &g, &b);
    led_color_t color = {(uint8_t)r, (uint8_t)g, (uint8_t)b};

    if (strcmp(type_item->valuestring, "info") == 0)
    {
        led_db_add_info(name_item->valuestring, color); // Passage du type led_color_t
    }
    else
    {
        int blinks = blinks_item ? blinks_item->valueint : 1;
        led_db_add_alarm(name_item->valuestring, blinks, color); // Passage du type led_color_t
    }

    httpd_resp_sendstr(req, "{\"status\":\"added\"}");
    cJSON_Delete(root);
    return ESP_OK;
}

// --- 5. POST /api/led/delete ---
static esp_err_t led_delete_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *name_item = cJSON_GetObjectItem(root, "name");

    if (name_item && name_item->valuestring) {
        led_db_delete_by_name(name_item->valuestring); 
        httpd_resp_sendstr(req, "{\"status\":\"deleted\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nom manquant");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// --- 6. POST /api/led/preview ---
static esp_err_t led_preview_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    
    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *rj = cJSON_GetObjectItem(json, "r");
        cJSON *gj = cJSON_GetObjectItem(json, "g");
        cJSON *bj = cJSON_GetObjectItem(json, "b");

        if (rj && gj && bj) {
            led_color_t color = {(uint8_t)rj->valueint, (uint8_t)gj->valueint, (uint8_t)bj->valueint};
            // On utilise led_set_background pour un effet immédiat en mode fixe
            led_set_background(LED_MODE_FIXED, color, 0);
            ESP_LOGI("PREVIEW", "R:%d G:%d B:%d", color.r, color.g, color.b);
        }
        cJSON_Delete(json);
    }
    
    httpd_resp_send(req, NULL, 0); 
    return ESP_OK;
}

esp_err_t ws_register_led_api(httpd_handle_t server)
{
    httpd_uri_t led_status_uri = {.uri = "/api/led", .method = HTTP_GET, .handler = led_status_get_handler};
    httpd_register_uri_handler(server, &led_status_uri);

    httpd_uri_t led_sim_uri = {.uri = "/api/led/simulate", .method = HTTP_POST, .handler = led_simulate_post_handler};
    httpd_register_uri_handler(server, &led_sim_uri);

    httpd_uri_t led_off_uri = {.uri = "/api/led/off", .method = HTTP_POST, .handler = led_off_post_handler};
    httpd_register_uri_handler(server, &led_off_uri);

    httpd_uri_t led_add_uri = {.uri = "/api/led/add", .method = HTTP_POST, .handler = led_add_post_handler};
    httpd_register_uri_handler(server, &led_add_uri);

    httpd_uri_t led_delete_uri = {.uri = "/api/led/delete", .method = HTTP_POST, .handler = led_delete_post_handler};
    httpd_register_uri_handler(server, &led_delete_uri);

    httpd_uri_t led_preview = {.uri = "/api/led/preview", .method = HTTP_POST, .handler = led_preview_post_handler};
    httpd_register_uri_handler(server, &led_preview);
    
    ESP_LOGI(TAG, "API LED enregistrée.");
    return ESP_OK;
}
