#include <esp_http_server.h>
#include "ws_api_led.h"
#include "cJSON.h"
// #include "led_ctrl.h"
#include "esp_log.h"
#include <string.h> // Ajouté pour strcmp

static const char *TAG = "WS_API_LED";

// --- Vérifier si un nom existe déjà ---
bool name_exists(const char *name)
{
    for (int i = 0; i < led_db_get_info_count(); i++)
    {
        if (strcmp(led_db_get_info_by_idx(i)->name, name) == 0)
            return true;
    }
    for (int i = 0; i < led_db_get_alarm_count(); i++)
    {
        if (strcmp(led_db_get_alarm_by_idx(i)->name, name) == 0)
            return true;
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
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalide");
        return ESP_FAIL;
    }

    cJSON *info_idx_item = cJSON_GetObjectItem(root, "info_idx");
    cJSON *alarm_idx_item = cJSON_GetObjectItem(root, "alarm_idx");

    // On vérifie qu'au moins un des deux est présent
    if (info_idx_item || alarm_idx_item)
    {
        // On récupère les valeurs (ou -1 si absent pour que led_db_simulate sache quoi ignorer)
        int info_idx = info_idx_item ? info_idx_item->valueint : -1;
        int alarm_idx = alarm_idx_item ? alarm_idx_item->valueint : -1;

        ESP_LOGI(TAG, "Web Simulate -> Info: %d, Alarme: %d", info_idx, alarm_idx);
        
        // Appelle ta fonction de simulation
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
    led_clear_alarms();
    led_color_t black = {0, 0, 0};
    led_set_effect(LED_MODE_FIXED, black, 0, 0);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"off\"}");
    return ESP_OK;
}

// --- 4. POST /api/led/add ---
static esp_err_t led_add_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return ESP_FAIL;

    cJSON *name_item = cJSON_GetObjectItem(root, "name");
    cJSON *color_item = cJSON_GetObjectItem(root, "color");
    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    cJSON *blinks_item = cJSON_GetObjectItem(root, "blinks");

    // 1. Vérification de la présence des champs obligatoires
    if (!name_item || !color_item || !type_item || !name_item->valuestring)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Données incomplètes");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 2. Vérification des doublons (on passe la chaîne .valuestring)
    if (name_exists(name_item->valuestring))
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Le nom existe déjà");
        cJSON_Delete(root); // Ne pas oublier de libérer ici !
        return ESP_FAIL;
    }

    if (name_item && color_item && type_item)
    {
        int r, g, b;
        sscanf(color_item->valuestring, "#%02x%02x%02x", &r, &g, &b);

        if (strcmp(type_item->valuestring, "info") == 0)
        {
            led_db_add_info(name_item->valuestring, (uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
        else
        {
            int blinks = blinks_item ? blinks_item->valueint : 1;
            led_db_add_alarm(name_item->valuestring, blinks, (uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
        httpd_resp_sendstr(req, "{\"status\":\"added\"}");
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Données incomplètes");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t led_delete_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *name_item = cJSON_GetObjectItem(root, "name");

    if (name_item && name_item->valuestring) {
        // Appelle ta fonction de suppression dans led_ctrl.c
        led_db_delete_by_name(name_item->valuestring); 
        ESP_LOGI("API", "Suppression de : %s", name_item->valuestring);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"deleted\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nom manquant");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// Handler simplifié pour le temps réel
static esp_err_t led_preview_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0'; // Toujours terminer la chaîne
    
    cJSON *json = cJSON_Parse(buf);
    if (json) {
        // Extraction avec pointeurs pour vérification de sécurité
        cJSON *rj = cJSON_GetObjectItem(json, "r");
        cJSON *gj = cJSON_GetObjectItem(json, "g");
        cJSON *bj = cJSON_GetObjectItem(json, "b");

        if (rj && gj && bj) {
            int r = rj->valueint;
            int g = gj->valueint;
            int b = bj->valueint;

            // Application immédiate
            led_set_bg_mode(LED_MODE_FIXED); 
            led_set_bg_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
            
            // Correction du LOG : on utilise directement les variables r, g, b
            ESP_LOGI("PREVIEW", "R:%d G:%d B:%d", r, g, b);
        }

        cJSON_Delete(json);
    }
    
    httpd_resp_send(req, NULL, 0); 
    return ESP_OK;
}

// --- 5. Fonction d'enregistrement (Placée à la fin) ---
void ws_register_led_api(httpd_handle_t server)
{
    httpd_uri_t led_status_uri = {.uri = "/api/led", .method = HTTP_GET, .handler = led_status_get_handler};
    httpd_register_uri_handler(server, &led_status_uri);

    httpd_uri_t led_sim_uri = {.uri = "/api/led/simulate", .method = HTTP_POST, .handler = led_simulate_post_handler};
    httpd_register_uri_handler(server, &led_sim_uri);

    httpd_uri_t led_off_uri = {.uri = "/api/led/off", .method = HTTP_POST, .handler = led_off_post_handler};
    httpd_register_uri_handler(server, &led_off_uri);

    httpd_uri_t led_add_uri = {.uri = "/api/led/add", .method = HTTP_POST, .handler = led_add_post_handler};
    httpd_register_uri_handler(server, &led_add_uri);

    httpd_uri_t led_delete_uri = {.uri = "/api/led/delete",.method   = HTTP_POST,.handler  = led_delete_post_handler,.user_ctx = NULL};
    httpd_register_uri_handler(server, &led_delete_uri);

    httpd_uri_t led_preview = {.uri = "/api/led/preview",.method    = HTTP_POST,.handler   = led_preview_post_handler,.user_ctx  = NULL};
    httpd_register_uri_handler(server, &led_preview);
    
    ESP_LOGI(TAG, "API LED (GET/POST/OFF/ADD) enregistrée.");
}

