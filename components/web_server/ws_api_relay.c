#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "relay.h"
#include <string.h>
#include "ws_api_relay.h"
#include "web_server_metrics.h"

static const char *TAG = "WS_API_RELAY";

/* -------------------------------------------------------------------------- */
/*  HELPER : ENVOI JSON                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Envoie une réponse JSON avec CORS
 */
static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

/* -------------------------------------------------------------------------- */
/*  GET /api/relay                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Renvoie l'état complet du relais (runtime + config)
 */
static esp_err_t relay_get_handler(httpd_req_t *req)
{
    char *json = relay_get_json_status();
    if (!json)
        return httpd_resp_send_500(req);

    esp_err_t res = send_json(req, json);
    free(json);
    return res;
}

/* -------------------------------------------------------------------------- */
/*  POST /api/relay                                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Reçoit un JSON de configuration dynamique
 *
 * Exemple :
 * {
 *   "state": true,
 *   "gpio": 4,
 *   "min_delay": 30,
 *   "inverted": false
 * }
 */
static esp_err_t relay_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0)
        return ESP_FAIL;

    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return httpd_resp_send_500(req);

    bool has_error = false;
    char error_msg[128] = {0};

    /* ---------------------------------------------------------------------- */
    /*  STATE                                                                 */
    /* ---------------------------------------------------------------------- */
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsBool(state))
    {
        bool target = cJSON_IsTrue(state);
        bool before = g_relay_runtime.state;

        relay_set(target);

        // Si l'état n'a pas changé → erreur délai trop court
        if (before == g_relay_runtime.state &&
            strlen(g_relay_runtime.last_error) > 0)
        {
            has_error = true;
            snprintf(error_msg, sizeof(error_msg),
                     "%s", g_relay_runtime.last_error);
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  MIN_DELAY                                                             */
    /* ---------------------------------------------------------------------- */
    cJSON *min_delay = cJSON_GetObjectItem(root, "min_delay");
    if (cJSON_IsNumber(min_delay))
    {
        uint32_t new_delay = (uint32_t)min_delay->valuedouble;
        g_relay_runtime.min_delay_s = new_delay;
        ESP_LOGI(TAG, "min_delay mis à %u sec", new_delay);
    }

    /* ---------------------------------------------------------------------- */
    /*  GPIO                                                                  */
    /* ---------------------------------------------------------------------- */
    cJSON *gpio = cJSON_GetObjectItem(root, "gpio");
    if (cJSON_IsNumber(gpio))
    {
        int new_gpio = gpio->valueint;
        if (relay_set_gpio(new_gpio) != ESP_OK)
        {
            has_error = true;
            snprintf(error_msg, sizeof(error_msg),
                     "GPIO %d invalide", new_gpio);
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  INVERTED                                                              */
    /* ---------------------------------------------------------------------- */
    cJSON *inv = cJSON_GetObjectItem(root, "inverted");
    if (cJSON_IsBool(inv))
    {
        bool new_inv = cJSON_IsTrue(inv);
        relay_set_inverted(new_inv);
    }

    cJSON_Delete(root);

    /* ---------------------------------------------------------------------- */
    /*  RÉPONSE JSON                                                          */
    /* ---------------------------------------------------------------------- */

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", !has_error);
    cJSON_AddStringToObject(resp, "error", has_error ? error_msg : "");

    // Ajouter l'état complet du relais
    char *status = relay_get_json_status();
    cJSON *status_json = cJSON_Parse(status);
    free(status);

    cJSON_AddItemToObject(resp, "relay", status_json);

    char *out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    esp_err_t res = send_json(req, out);
    free(out);
    return res;
}

/* -------------------------------------------------------------------------- */
/*  POST /api/relay/on                                                        */
/* -------------------------------------------------------------------------- */

static esp_err_t relay_on_handler(httpd_req_t *req)
{
    relay_on();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddBoolToObject(resp, "state", true);

    char *out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    esp_err_t res = send_json(req, out);
    free(out);
    return res;
}

/* -------------------------------------------------------------------------- */
/*  POST /api/relay/off                                                       */
/* -------------------------------------------------------------------------- */

static esp_err_t relay_off_handler(httpd_req_t *req)
{
    relay_off();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddBoolToObject(resp, "state", false);

    char *out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    esp_err_t res = send_json(req, out);
    free(out);
    return res;
}

/* -------------------------------------------------------------------------- */
/*  ENREGISTREMENT DES ROUTES                                                */
/* -------------------------------------------------------------------------- */
esp_err_t ws_register_relay_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_RELAY: START REGISTER ===");

    g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    esp_err_t err;

    // ---------------- GET STATE ----------------
    httpd_uri_t get_uri = {
        .uri = "/api/relay",
        .method = HTTP_GET,
        .handler = relay_get_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET)", get_uri.uri);

    err = httpd_register_uri_handler(server, &get_uri);
    ESP_LOGI(TAG, "Result GET /relay -> %s", esp_err_to_name(err));

    // ---------------- POST STATE ----------------
    httpd_uri_t post_uri = {
        .uri = "/api/relay",
        .method = HTTP_POST,
        .handler = relay_post_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST)", post_uri.uri);

    err = httpd_register_uri_handler(server, &post_uri);
    ESP_LOGI(TAG, "Result POST /relay -> %s", esp_err_to_name(err));

    // ---------------- ON ----------------
    httpd_uri_t on_uri = {
        .uri = "/api/relay/on",
        .method = HTTP_POST,
        .handler = relay_on_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST ON)", on_uri.uri);

    err = httpd_register_uri_handler(server, &on_uri);
    ESP_LOGI(TAG, "Result /on -> %s", esp_err_to_name(err));

    // ---------------- OFF ----------------
    httpd_uri_t off_uri = {
        .uri = "/api/relay/off",
        .method = HTTP_POST,
        .handler = relay_off_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST OFF)", off_uri.uri);

    err = httpd_register_uri_handler(server, &off_uri);
    ESP_LOGI(TAG, "Result /off -> %s", esp_err_to_name(err));

    // ---------------- FINAL ----------------

    g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_RELAY: END REGISTER ===");

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Relay API registration FAILED");
        return err;
    }

    ESP_LOGI(TAG, "API Relay enregistrée avec succès");
    return ESP_OK;
}
