#include "ws_api_led.h"
#include "led_service.h"
#include "led_db.h"    /* Pour led_db_exists, led_db_add_info, led_db_add_alarm */
#include "led_types.h" /* Pour le type led_color_t */
#include "led_ctrl.h"  /* Pour led_stop */
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "WS_API_LED";

// --- 1. GET /api/led ---
static esp_err_t led_status_get_handler(httpd_req_t *req)
{
    cJSON *json = led_db_get_json_status();
    if (!json)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get LED status");

    char *out = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);

    free(out);
    cJSON_Delete(json);
    return ESP_OK;
}

// --- 2. POST /api/led/add ---
static esp_err_t led_add_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *name = cJSON_GetObjectItem(root, "name");
    cJSON *color_hex = cJSON_GetObjectItem(root, "color");
    cJSON *type = cJSON_GetObjectItem(root, "type");

    if (!cJSON_IsString(name) || !cJSON_IsString(color_hex) || !cJSON_IsString(type))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
    }

    if (led_db_exists(name->valuestring))
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Name already exists");
    }

    int r, g, b;
    // Sécurité : on vérifie que sscanf a bien trouvé les 3 variables
    if (sscanf(color_hex->valuestring, "#%02x%02x%02x", &r, &g, &b) != 3)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid color format");
    }

    led_color_t col = {(uint8_t)r, (uint8_t)g, (uint8_t)b};

    if (strcmp(type->valuestring, "info") == 0)
    {
        led_db_add_info(name->valuestring, col);
    }
    else
    {
        cJSON *blinks = cJSON_GetObjectItem(root, "blinks");
        led_db_add_alarm(name->valuestring, cJSON_IsNumber(blinks) ? blinks->valueint : 1, col);
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json"); // Propre pour le front-end
    httpd_resp_sendstr(req, "{\"status\":\"added\"}");
    return ESP_OK;
}

// --- 3. POST /api/led/off ---
static esp_err_t led_off_post_handler(httpd_req_t *req)
{
    led_stop();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"off\"}");
    return ESP_OK;
}

// --- 5. POST /api/led/delete ---
static esp_err_t led_delete_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *name_item = cJSON_GetObjectItem(root, "name");

    if (name_item && name_item->valuestring)
    {
        led_db_delete_by_name(name_item->valuestring);
        httpd_resp_sendstr(req, "{\"status\":\"deleted\"}");
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nom manquant");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// --- 6. POST /api/led/preview ---
static esp_err_t led_preview_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json)
    {
        cJSON *rj = cJSON_GetObjectItem(json, "r");
        cJSON *gj = cJSON_GetObjectItem(json, "g");
        cJSON *bj = cJSON_GetObjectItem(json, "b");

        if (rj && gj && bj)
        {
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

// --- 7. POST /api/led/simulate
static esp_err_t led_simulate_post_handler(httpd_req_t *req)
{
    char buf[150];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalide");
        return ESP_FAIL;
    }

    cJSON *info_idx_item = cJSON_GetObjectItem(root, "info_idx");
    cJSON *alarm_idx_item = cJSON_GetObjectItem(root, "alarm_idx");

    // Utilisation de variables locales pour clarifier
    int info_idx = -1;
    int alarm_idx = -1;
    bool valid = false;

    if (cJSON_IsNumber(info_idx_item)) {
        info_idx = info_idx_item->valueint;
        valid = true;
    }
    
    if (cJSON_IsNumber(alarm_idx_item)) {
        alarm_idx = alarm_idx_item->valueint;
        valid = true;
    }

    if (valid) {
        ESP_LOGI(TAG, "Web Simulate -> Info: %d, Alarme: %d", info_idx, alarm_idx);
        
        // Exécution de la simulation
        led_db_simulate(info_idx, alarm_idx);
        
        // Réponse JSON propre
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } 
    else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Aucun index valide fourni");
    }

    // Libération systématique de la mémoire
    cJSON_Delete(root);
    return ESP_OK;
}

// --- Enregistrement ---
esp_err_t ws_register_led_api(httpd_handle_t server)
{
    // Utilisation d'un tableau pour la lisibilité
    httpd_uri_t routes[] = {
        {.uri = "/api/led", .method = HTTP_GET, .handler = led_status_get_handler, .user_ctx = NULL},
        {.uri = "/api/led/add", .method = HTTP_POST, .handler = led_add_post_handler, .user_ctx = NULL},
        {.uri = "/api/led/off", .method = HTTP_POST, .handler = led_off_post_handler, .user_ctx = NULL},
        {.uri = "/api/led/simulate", .method = HTTP_POST, .handler = led_simulate_post_handler, .user_ctx = NULL},
        {.uri = "/api/led/delete", .method = HTTP_POST, .handler = led_delete_post_handler, .user_ctx = NULL},
        {.uri = "/api/led/preview", .method = HTTP_POST, .handler = led_preview_post_handler, .user_ctx = NULL}
};

for (int i = 0; i < sizeof(routes) / sizeof(httpd_uri_t); i++)
{
    httpd_register_uri_handler(server, &routes[i]);
}

ESP_LOGI(TAG, "API LED enregistrées");
return ESP_OK;
}
