#include "ws_api_prediction.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_http_server.h"

#include "prediction_engine.h"  
#include "cJSON.h"

static const char *TAG = "WS_PREDICT";

/* =========================================================
 * GET /api/predict/status
 * ========================================================= */
static esp_err_t predict_status_handler(httpd_req_t *req)
{
    char *json = prediction_engine_get_json_status();
    if (!json) {
        ESP_LOGE(TAG, "Erreur génération JSON");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);
    return err;
}

/* =========================================================
 * POST /api/predict/cmd
 * (optionnel selon ton moteur)
 * ========================================================= */
static esp_err_t predict_command_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }

    buf[len] = '\0';
    ESP_LOGI(TAG, "CMD: %s", buf);

    /* Exemple de commandes :
       - "RESET"
       - "RECALC"
       - "SET:window=48"
    */

    if (strcmp(buf, "RESET") == 0) {
        prediction_engine_reset();
    }
    else if (strcmp(buf, "RECALC") == 0) {
        prediction_engine_force_recompute();
    }
    else if (strncmp(buf, "SET:", 4) == 0) {
        prediction_engine_parse_command(buf + 4);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* =========================================================
 * REGISTER
 * ========================================================= */
esp_err_t ws_register_prediction_api(httpd_handle_t server)
{
    httpd_uri_t uri_status = {
        .uri = "/api/predict/status",
        .method = HTTP_GET,
        .handler = predict_status_handler,
        .user_ctx = NULL
    };

    httpd_uri_t uri_cmd = {
        .uri = "/api/predict/cmd",
        .method = HTTP_POST,
        .handler = predict_command_handler,
        .user_ctx = NULL
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_status));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_cmd));

    ESP_LOGI(TAG, "Prediction API registered");
    return ESP_OK;
}
