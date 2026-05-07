#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "heating_program.h"
#include <string.h>

static const char *TAG = "WS_API_PROG";

// [GET] /api/program/get
static esp_err_t get_program_handler(httpd_req_t *req)
{
    char *json_string = heating_get_json(&config);
    if (json_string == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t res = httpd_resp_send(req, json_string, strlen(json_string));
    free(json_string); 
    return res;
}

// [POST] /api/program/set
static esp_err_t set_program_handler(httpd_req_t *req)
{
    char content[150];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) return ESP_FAIL;
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON Invalide");
        return ESP_FAIL;
    }

    cJSON *day = cJSON_GetObjectItem(root, "day");
    cJSON *idx = cJSON_GetObjectItem(root, "idx");
    cJSON *h   = cJSON_GetObjectItem(root, "h");
    cJSON *m   = cJSON_GetObjectItem(root, "m");
    cJSON *s   = cJSON_GetObjectItem(root, "s");
    cJSON *t   = cJSON_GetObjectItem(root, "t");

    if (day && idx && h && m && s && t) {
        heating_set_point(&config, day->valueint, idx->valueint, h->valueint, m->valueint, s->valueint, (float)t->valuedouble);
        heating_save(&config);
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
}

// [GET] /api/program/reset
static esp_err_t reset_program_handler(httpd_req_t *req)
{
    heating_reset_defaults(&config);
    heating_save(&config);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"reset_done\"}", 23);
}

// Enregistrement des routes
esp_err_t ws_register_program_api(httpd_handle_t server)
{
    httpd_uri_t uri_get = { .uri = "/api/program/get", .method = HTTP_GET, .handler = get_program_handler };
    httpd_uri_t uri_set = { .uri = "/api/program/set", .method = HTTP_POST, .handler = set_program_handler };
    httpd_uri_t uri_reset = { .uri = "/api/program/reset", .method = HTTP_GET, .handler = reset_program_handler };

    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_set);
    httpd_register_uri_handler(server, &uri_reset);

    ESP_LOGI(TAG, "API Program OK");
    return ESP_OK;
}