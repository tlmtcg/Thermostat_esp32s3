#include "ws_api_sht31.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "i2c_manager.h"
#include "sht31.h"
#include "config_runtime.h"

static const char *TAG = "WS_API_SHT31";

static esp_err_t sht31_handler(httpd_req_t *req)
{
    char *json = sht31_get_json_status();
    if (!json)
        return httpd_resp_send_500(req);

    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, json, strlen(json));

    free(json);
    return res;
}

// Handler POST pour mettre à jour la configuration
esp_err_t sht31_config_post_handler(httpd_req_t *req)
{
    char buf[150];
    int ret, remaining = req->content_len;

    // 1. Sécurité de taille du tampon
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON trop grand");
        return ESP_FAIL;
    }

    // 2. Lecture du contenu du POST
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur réception");
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // Fin de chaîne

    // 3. Parsing du JSON reçu
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON Invalide");
        return ESP_FAIL;
    }

    // Récupération de la config runtime actuelle
    sht31_config_t current_config;
    sht31_get_config(&current_config);

    // Extraction des champs du JSON
    cJSON *addr = cJSON_GetObjectItem(root, "addr");
    cJSON *interval = cJSON_GetObjectItem(root, "read_interval_ms");
    cJSON *log_sd = cJSON_GetObjectItem(root, "log_to_sd");

    if (addr)     current_config.addr = (uint8_t)addr->valueint;
    if (interval) current_config.read_interval_ms = (uint32_t)interval->valueint;
    if (log_sd)   current_config.log_to_sd = cJSON_IsTrue(log_sd);

    cJSON_Delete(root);

    // 4. Application de la nouvelle config en RAM
    sht31_set_config(&current_config);
    g_cfg.sht31_addr = current_config.addr;
    g_cfg.sht31_read_interval_ms = current_config.read_interval_ms;
    g_cfg.sht31_log_to_sd = current_config.log_to_sd;

    // 5. Sauvegarde immédiate en nvs
    config_runtime_save();


    // 6. Réponse au client web
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"OK\",\"message\":\"Configuration SHT31 appliquée\"}");
    return ESP_OK;
}

esp_err_t ws_register_sht31_api(httpd_handle_t server)
{
    // 1. Déclaration de l'URI GET pour la lecture des mesures (Runtime)
    httpd_uri_t uri_get = {
        .uri      = "/api/sensors/sht31",
        .method   = HTTP_GET,
        .handler  = sht31_handler,
        .user_ctx = NULL
    };

    // 2. Déclaration de l'URI POST pour la modification de la configuration
    httpd_uri_t uri_post = {
        .uri      = "/api/config/sht31",
        .method   = HTTP_POST,
        .handler  = sht31_config_post_handler, // Ton handler qui parse le cJSON
        .user_ctx = NULL
    };

    // Enregistrement du GET
    esp_err_t err = httpd_register_uri_handler(server, &uri_get);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement GET SHT31: %s", esp_err_to_name(err));
        return err;
    }

    // Enregistrement du POST
    err = httpd_register_uri_handler(server, &uri_post);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement POST SHT31: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "API SHT31 enregistree (GET /sensors + POST /config) : %s", esp_err_to_name(err));
    return ESP_OK;
}

esp_err_t i2c_manager_reinit(void)
{
    i2c_master_bus_handle_t bus = i2c_manager_get_bus();

    if (!bus)
    {
        ESP_LOGE(TAG, "Bus I2C non initialise");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
