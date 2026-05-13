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
extern ssd1306_t oled;

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
 *  JSON:
 *  {
 *    "x": 10,
 *    "y": 20,
 *    "text": "Hello"
 *  }
 * ========================================================= */
static esp_err_t oled_text_handler(httpd_req_t *req)
{
    char buf[256];

    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *x_json = cJSON_GetObjectItem(root, "x");
    cJSON *y_json = cJSON_GetObjectItem(root, "y");
    cJSON *text_json = cJSON_GetObjectItem(root, "text");

    if (!text_json) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing text");
    }

    int x = x_json ? x_json->valueint : 0;
    int y = y_json ? y_json->valueint : 0;
    const char *text = text_json->valuestring;

    ESP_LOGI(TAG, "OLED text: (%d,%d) %s", x, y, text);

    ssd1306_draw_string(&oled, x, y, text);
    ssd1306_update(&oled);

    cJSON_Delete(root);

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  POST /api/oled/line
 *  {
 *    "line": 2,
 *    "text": "Temp: 23C"
 *  }
 * ========================================================= */
static esp_err_t oled_line_handler(httpd_req_t *req)
{
    char buf[256];

    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    cJSON *line_json = cJSON_GetObjectItem(root, "line");
    cJSON *text_json = cJSON_GetObjectItem(root, "text");

    if (!line_json || !text_json) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
    }

    int line = line_json->valueint;
    const char *text = text_json->valuestring;

    ESP_LOGI(TAG, "OLED line %d: %s", line, text);

    ssd1306_write_line(&oled, line, text);
    ssd1306_update(&oled);

    cJSON_Delete(root);

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
    if (len <= 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    cJSON *line_json = cJSON_GetObjectItem(root, "line");

    if (!line_json) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing line");
    }

    int line = line_json->valueint;

    ssd1306_write_line(&oled, line, "                ");
    ssd1306_update(&oled);

    cJSON_Delete(root);

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* =========================================================
 *  REGISTER API
 * ========================================================= */
esp_err_t ws_register_ssd1306_api(httpd_handle_t server)
{
    ESP_LOGI(TAG, "Register OLED API");

    httpd_uri_t clear = {
        .uri = "/api/oled/clear",
        .method = HTTP_POST,
        .handler = oled_clear_handler
    };

    httpd_uri_t text = {
        .uri = "/api/oled/text",
        .method = HTTP_POST,
        .handler = oled_text_handler
    };

    httpd_uri_t line = {
        .uri = "/api/oled/line",
        .method = HTTP_POST,
        .handler = oled_line_handler
    };

    httpd_uri_t clear_line = {
        .uri = "/api/oled/clear-line",
        .method = HTTP_POST,
        .handler = oled_clear_line_handler
    };

    httpd_register_uri_handler(server, &clear);
    httpd_register_uri_handler(server, &text);
    httpd_register_uri_handler(server, &line);
    httpd_register_uri_handler(server, &clear_line);

    return ESP_OK;
}
