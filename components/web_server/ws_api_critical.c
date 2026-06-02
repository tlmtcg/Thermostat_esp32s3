#include "ws_api_critical.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "cJSON.h"
#include "thermostat.h"
#include "sht31.h"
#include "temperature.h"
#include "config_runtime.h"

static const char *TAG = "WS_API_CRITICAL";

// Handler GET pour récupérer l'état actuel (thermostat, capteurs et config de secours)
static esp_err_t critical_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return httpd_resp_send_500(req);

    // 1. Récupération des données du thermostat et des capteurs
    const sht31_runtime_t *sht_runtime = sht31_get_runtime();
    float ext_temp = temperature_get_outdoor();
    bool ext_valid = (!isnan(ext_temp) && ext_temp > -40.0f && ext_temp < 60.0f);

    // Injection de l'état dynamique des capteurs
    cJSON_AddBoolToObject(root, "indoor_valid", sht_runtime->valid);
    cJSON_AddNumberToObject(root, "indoor_temp", sht_runtime->temperature);
    cJSON_AddBoolToObject(root, "outdoor_valid", ext_valid);
    cJSON_AddNumberToObject(root, "outdoor_temp", ext_valid ? ext_temp : 0.0f);
    
    // Mode textuel ou brut selon votre structure g_thermostat_runtime
    cJSON_AddStringToObject(root, "mode", "HORS_GEL");
    cJSON_AddStringToObject(root, "mode_text", "Mode Hors-Gel Actif");

    // 2. Injection de la configuration critique persistante actuelle (g_cfg)
    cJSON_AddNumberToObject(root, "secu_cycle_duration_sec", g_cfg.secu_cycle_duration_sec);
    cJSON_AddNumberToObject(root, "fallback_duty_percent", g_cfg.fallback_duty_percent);
    cJSON_AddNumberToObject(root, "extreme_ext_temp", g_cfg.extreme_ext_temp);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) return httpd_resp_send_500(req);

    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, json, strlen(json));

    free(json);
    return res;
}

// Handler POST pour mettre à jour les paramètres de secours du mode critique
static esp_err_t critical_config_post_handler(httpd_req_t *req)
{
    char buf[256];
    int remaining = req->content_len;

    // Sécurité stricte du buffer (conservation de 1 octet pour le terminal '\0')
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON trop grand");
        return ESP_FAIL;
    }

    // Boucle de réception obligatoire pour garantir la lecture totale du flux HTTP
    int received = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Timeout temporaire, on retente
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur réception");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0'; // Fin de chaîne sécurisée pour cJSON

    // Extraction et Parsing du JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON Invalide");
        return ESP_FAIL;
    }

    cJSON *cycle = cJSON_GetObjectItem(root, "secu_cycle_duration_sec");
    cJSON *duty = cJSON_GetObjectItem(root, "fallback_duty_percent");
    cJSON *extreme = cJSON_GetObjectItem(root, "extreme_ext_temp");

    // Mise à jour sécurisée des variables globales de stockage (g_cfg)
    if (cycle)   g_cfg.secu_cycle_duration_sec = (uint32_t)cycle->valueint;
    if (duty)    g_cfg.fallback_duty_percent = (uint32_t)duty->valueint;
    if (extreme) g_cfg.extreme_ext_temp = (float)extreme->valuedouble;

    cJSON_Delete(root);

    // Sauvegarde immédiate dans la mémoire persistante NVS
    config_runtime_save();

    ESP_LOGI(TAG, "Config Critique appliquee : Cycle=%lus, Duty=%lu%%, ExtExtreme=%.1f C",
             (unsigned long)g_cfg.secu_cycle_duration_sec,
             (unsigned long)g_cfg.fallback_duty_percent,
             g_cfg.extreme_ext_temp);

    // Réponse de confirmation au client web
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"OK\",\"message\":\"Configuration mode secours appliquée avec succès\"}");
    return ESP_OK;
}

// Fonction d'enregistrement publique de l'API critique
esp_err_t ws_register_critical_api(httpd_handle_t server)
{
    httpd_uri_t uri_get = {
        .uri      = "/api/thermostat/status",
        .method   = HTTP_GET,
        .handler  = critical_status_handler,
        .user_ctx = NULL
    };

    httpd_uri_t uri_post = {
        .uri      = "/api/thermostat/config/critical",
        .method   = HTTP_POST,
        .handler  = critical_config_post_handler,
        .user_ctx = NULL
    };

    esp_err_t err = httpd_register_uri_handler(server, &uri_get);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement GET Status Critique: %s", esp_err_to_name(err));
        return err;
    }

    err = httpd_register_uri_handler(server, &uri_post);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement POST Config Critique: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "API Mode Critique enregistrée (GET /api/thermostat/status + POST /api/thermostat/config/critical)");
    return ESP_OK;
}
