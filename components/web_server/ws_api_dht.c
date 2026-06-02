#include "ws_api_dht.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "cJSON.h"
#include "dht.h"
#include "config_runtime.h"

static const char *TAG = "WS_API_DHT";

// Handler GET pour récupérer l'état actuel et les mesures au format JSON
static esp_err_t dht_handler(httpd_req_t *req)
{
    char *json = dht_get_json_status();
    if (!json)
        return httpd_resp_send_500(req);

    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, json, strlen(json));

    free(json);
    return res;
}

// Handler POST pour mettre à jour la configuration du DHT
esp_err_t dht_config_post_handler(httpd_req_t *req)
{
    // CORRECTION 1 : Augmenter légèrement la taille à 256 car le JSON brut de configuration 
    // + les structures HTTP peuvent vite saturer un buffer trop petit.
    char buf[256];
    int remaining = req->content_len;

    // CORRECTION 2 : Sécurité stricte du buffer. On garde 1 octet pour le '\0'.
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON trop grand");
        return ESP_FAIL;
    }

    // CORRECTION 3 : Boucle de réception obligatoire pour garantir la lecture totale du flux HTTP
    int received = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Timeout temporaire, on réessaie
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur réception");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0'; // Fin de chaîne sécurisée pour cJSON

    // 3. Parsing du JSON reçu
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON Invalide");
        return ESP_FAIL;
    }

    // Récupération de la configuration actuelle du DHT
    dht_config_t current_config;
    dht_get_config(&current_config);

    // Extraction des champs spécifiques au DHT depuis le JSON
    cJSON *gpio = cJSON_GetObjectItem(root, "gpio_pin");
    cJSON *type = cJSON_GetObjectItem(root, "sensor_type");
    cJSON *interval = cJSON_GetObjectItem(root, "read_interval_ms");
    cJSON *log_sd = cJSON_GetObjectItem(root, "log_to_sd");

    if (gpio)     current_config.gpio_pin = (gpio_num_t)gpio->valueint;
    if (type)     current_config.sensor_type = (dht_sensor_type_t)type->valueint;
    if (interval) current_config.read_interval_ms = (uint32_t)interval->valueint;
    if (log_sd)   current_config.log_to_sd = cJSON_IsTrue(log_sd);

    cJSON_Delete(root);

    // 4. Application de la nouvelle configuration au module DHT
    dht_set_config(&current_config);
    
    // Mises à jour des variables globales de ton stockage de configuration g_cfg
    g_cfg.dht_gpio_pin = current_config.gpio_pin;
    g_cfg.dht_sensor_type = current_config.sensor_type;
    g_cfg.dht_read_int_ms = current_config.read_interval_ms;
    g_cfg.dht_log_to_sd = current_config.log_to_sd;

    // 5. Sauvegarde immédiate dans la mémoire NVS
    config_runtime_save();

    // 6. Réponse de confirmation au client web
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"OK\",\"message\":\"Configuration DHT appliquée avec succès\"}");
    return ESP_OK;
}

// Fonction d'enregistrement publique de l'API
esp_err_t ws_register_dht_api(httpd_handle_t server)
{
    httpd_uri_t uri_get = {
        .uri      = "/api/sensors/dht",
        .method   = HTTP_GET,
        .handler  = dht_handler,
        .user_ctx = NULL
    };

    httpd_uri_t uri_post = {
        .uri      = "/api/config/dht",
        .method   = HTTP_POST,
        .handler  = dht_config_post_handler,
        .user_ctx = NULL
    };

    esp_err_t err = httpd_register_uri_handler(server, &uri_get);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement GET DHT: %s", esp_err_to_name(err));
        return err;
    }

    err = httpd_register_uri_handler(server, &uri_post);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement POST DHT: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "API DHT enregistrée avec succès (GET /sensors/dht + POST /config/dht)");
    return ESP_OK;
}
