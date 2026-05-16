#include "ws_api_u8g2.h" 
#include "u8g2.h"        

#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WS_API_U8G2";

/* =========================================================
 *  Référence écran OLED U8g2 externe
 * ========================================================= */
extern u8g2_t u8g2;

/* =========================================================
 *  POST /api/oled/clear
 * ========================================================= */
static esp_err_t oled_clear_handler(httpd_req_t *req)
{
    u8g2_ClearBuffer(&u8g2); // Correction casse
    u8g2_SendBuffer(&u8g2);  // Correction casse

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

    uint8_t x = x_json ? x_json->valueint : 0;
    uint8_t y = y_json ? y_json->valueint : 10; 

    u8g2_DrawStr(&u8g2, x, y, text_json->valuestring); // Correction casse
    u8g2_SendBuffer(&u8g2);                           // Correction casse

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
        uint8_t y_position = (line_json->valueint * 12) + 12; 
        u8g2_DrawStr(&u8g2, 0, y_position, text_json->valuestring); // Correction casse
        u8g2_SendBuffer(&u8g2);                                   // Correction casse
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
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *inv_json = cJSON_GetObjectItem(root, "invert");

    if (inv_json) {
        // Correction casse : u8g2_SetDisplayRotation
        u8g2_SetDisplayRotation(&u8g2, cJSON_IsTrue(inv_json) ? U8G2_R0 : U8G2_R0); 
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/write
 * ========================================================= */
static esp_err_t oled_write_handler(httpd_req_t *req)
{
    u8g2_SendBuffer(&u8g2); // Correction casse
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
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *line_json = cJSON_GetObjectItem(root, "line");

    if (line_json) {
        uint8_t y_position = (line_json->valueint * 12) + 12;
        
        u8g2_SetDrawColor(&u8g2, 0); // Correction casse
        u8g2_DrawBox(&u8g2, 0, y_position - 10, 128, 12); // Correction casse : DrawBox au lieu de DrawRBox
        u8g2_SetDrawColor(&u8g2, 1); // Correction casse
        u8g2_SendBuffer(&u8g2);     // Correction casse
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  REGISTER API
 * ========================================================= */
esp_err_t ws_register_u8g2_api(httpd_handle_t server)
{
    ESP_LOGI(TAG, "Registering U8g2 OLED API endpoints...");

    httpd_uri_t oled_uris[] = {
        {"/api/oled/clear",      HTTP_POST, oled_clear_handler,      NULL},
        {"/api/oled/text",       HTTP_POST, oled_text_handler,       NULL},
        {"/api/oled/line",       HTTP_POST, oled_line_handler,       NULL},
        {"/api/oled/clear-line", HTTP_POST, oled_clear_line_handler, NULL},
        {"/api/oled/invert",     HTTP_POST, oled_invert_handler,     NULL},
        {"/api/oled/write",      HTTP_POST, oled_write_handler,      NULL}
    };

    for (int i = 0; i < sizeof(oled_uris)/sizeof(httpd_uri_t); i++) {
        httpd_register_uri_handler(server, &oled_uris[i]);
    }

    return ESP_OK;
}
