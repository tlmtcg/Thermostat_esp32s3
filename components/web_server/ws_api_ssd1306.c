#include "ws_api_ssd1306.h"
#include "ssd1306.h"

#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WS_API_OLED";

/* =========================================================
 *  Référence écran OLED (à initialiser ailleurs)
 * ========================================================= */


/* =========================================================
 *  POST /api/oled/clear
 * ========================================================= */
static esp_err_t oled_clear_handler(httpd_req_t *req)
{
    ssd1306_clear(&oled);
    ssd1306_update(&oled);

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/text
 * ========================================================= */
static esp_err_t oled_text_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *x_json = cJSON_GetObjectItem(root, "x");
    cJSON *y_json = cJSON_GetObjectItem(root, "y");
    cJSON *text_json = cJSON_GetObjectItem(root, "text");

    if (!text_json) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing text");
    }

    ssd1306_draw_string(&oled, x_json ? x_json->valueint : 0, y_json ? y_json->valueint : 0, text_json->valuestring);
    ssd1306_update(&oled);

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/line
 * ========================================================= */
static esp_err_t oled_line_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *line_json = cJSON_GetObjectItem(root, "line");
    cJSON *text_json = cJSON_GetObjectItem(root, "text");

    if (line_json && text_json) {
        ssd1306_write_line(&oled, line_json->valueint, text_json->valuestring);
        ssd1306_update(&oled);
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/invert
 *  JSON: {"invert": true}
 * ========================================================= */
static esp_err_t oled_invert_handler(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    cJSON *inv_json = cJSON_GetObjectItem(root, "invert");

    if (inv_json) {
        ssd1306_set_invert(&oled, cJSON_IsTrue(inv_json));
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/write
 *  Force un rafraîchissement complet ou écrit via buffer
 * ========================================================= */
static esp_err_t oled_write_handler(httpd_req_t *req)
{
    // On appelle simplement update pour valider le buffer actuel vers le matériel
    ssd1306_update(&oled);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/clear-line
 * ========================================================= */
static esp_err_t oled_clear_line_handler(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;

    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    cJSON *line_json = cJSON_GetObjectItem(root, "line");

    if (line_json) {
        ssd1306_write_line(&oled, line_json->valueint, "                ");
        ssd1306_update(&oled);
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  REGISTER API
 * ========================================================= */
esp_err_t ws_register_ssd1306_api(httpd_handle_t server)
{
    ESP_LOGI(TAG, "Registering OLED API endpoints...");

    httpd_uri_t oled_uris[] = {
        {"/api/oled/clear",      HTTP_POST, oled_clear_handler,      NULL},
        {"/api/oled/text",       HTTP_POST, oled_text_handler,       NULL},
        {"/api/oled/line",       HTTP_POST, oled_line_handler,       NULL},
        {"/api/oled/clear-line", HTTP_POST, oled_clear_line_handler, NULL},
        {"/api/oled/invert",     HTTP_POST, oled_invert_handler,     NULL}, // Nouvel endpoint
        {"/api/oled/write",      HTTP_POST, oled_write_handler,      NULL}  // Nouvel endpoint
    };

    for (int i = 0; i < sizeof(oled_uris)/sizeof(httpd_uri_t); i++) {
        httpd_register_uri_handler(server, &oled_uris[i]);
    }

    return ESP_OK;
}
