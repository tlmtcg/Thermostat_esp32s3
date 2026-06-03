#include "ws_api_critical.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "esp_log.h"
#include "cJSON.h"
#include "thermostat.h"
#include "sht31.h"
#include "config_runtime.h"

// Déclaration de la structure de runtime globale (état réel du thermostat)
extern thermostat_runtime_t g_thermostat_runtime;

// Déclaration des variables partagées gérées par le thermostat
extern bool g_critical_active;

static const char *TAG = "WS_API_CRITICAL";

// Handler GET pour récupérer l'état actuel (thermostat, capteurs et config de secours)
static esp_err_t critical_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return httpd_resp_send_500(req);

    // 1. Récupération directe des états issus des modules matériels et de configuration
    const sht31_runtime_t *sht_runtime = sht31_get_runtime();
    thermostat_config_t cfg;
    thermostat_get_config(&cfg);

    // L'API extrait directement la validité calculée en amont par le driver du capteur
    cJSON_AddBoolToObject(root,
                          "indoor_valid",
                          g_thermostat_runtime.temperature_valid);

    cJSON_AddNumberToObject(root,
                            "indoor_temp",
                            g_thermostat_runtime.temperature);

    // Pour l'extérieur, la validité dépend de la présence d'une valeur cohérente et non nulle
    bool ext_valid = !isnan(g_thermostat_runtime.temp_ext) && (g_thermostat_runtime.temp_ext != 0.0f);
    cJSON_AddBoolToObject(root, "outdoor_valid", ext_valid);
    cJSON_AddNumberToObject(root, "outdoor_temp", g_thermostat_runtime.temp_ext);

    // Injection des variables de secours issues de la structure centrale du thermostat
    cJSON_AddBoolToObject(root, "critical_active", g_critical_active || g_thermostat_runtime.critical_active);

    // CORRECTION : Lecture directe depuis la structure partagée modifiée par must_heat()
    cJSON_AddNumberToObject(root, "fallback_duty", (double)g_thermostat_runtime.fallback_duty);

    cJSON_AddBoolToObject(root, "relay_status", g_thermostat_runtime.state);

    // Gestion propre et dynamique des modes réels basés sur l'état système calculé par must_heat()
    if (g_critical_active || g_thermostat_runtime.critical_active)
    {
        cJSON_AddStringToObject(root, "mode", "CRITICAL");
        cJSON_AddStringToObject(root, "mode_text", "Sécurité : Mode Secours Actif");
    }
    else
    {
        switch (cfg.mode)
        {
        case THERMOSTAT_MODE_MANUAL:
            cJSON_AddStringToObject(root, "mode", "MANUAL");
            cJSON_AddStringToObject(root, "mode_text", "Mode Manuel");
            break;
        case THERMOSTAT_MODE_AUTO:
            cJSON_AddStringToObject(root, "mode", "AUTO");
            cJSON_AddStringToObject(root, "mode_text", "Mode Automatique");
            break;
        default:
            cJSON_AddStringToObject(root, "mode", "UNKNOWN");
            cJSON_AddStringToObject(root, "mode_text", "Mode Inconnu");
            break;
        }
    }

    // 2. Injection de la configuration critique persistante actuelle (g_cfg)
    cJSON_AddNumberToObject(root, "secu_cycle_duration_sec", g_cfg.secu_cycle_duration_sec);
    cJSON_AddNumberToObject(root, "fallback_duty_percent", g_cfg.fallback_duty_percent);
    cJSON_AddNumberToObject(root, "extreme_ext_temp", g_cfg.extreme_ext_temp);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json)
        return httpd_resp_send_500(req);

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

    if (remaining >= sizeof(buf))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON trop grand");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0)
    {
        int ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur réception");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON Invalide");
        return ESP_FAIL;
    }

    cJSON *cycle = cJSON_GetObjectItem(root, "secu_cycle_duration_sec");
    cJSON *duty = cJSON_GetObjectItem(root, "fallback_duty_percent");
    cJSON *extreme = cJSON_GetObjectItem(root, "extreme_ext_temp");

    if (cycle)
        g_cfg.secu_cycle_duration_sec = (uint32_t)cycle->valueint;
    if (duty)
        g_cfg.fallback_duty_percent = (uint32_t)duty->valueint;
    if (extreme)
        g_cfg.extreme_ext_temp = (float)extreme->valuedouble;

    cJSON_Delete(root);

    config_runtime_save();

    ESP_LOGI(TAG, "Config Critique appliquee : Cycle=%u s, Duty=%u%%, ExtExtreme=%.1f C",
             g_cfg.secu_cycle_duration_sec,
             g_cfg.fallback_duty_percent,
             g_cfg.extreme_ext_temp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"OK\",\"message\":\"Configuration mode secours appliquée avec succès\"}");
    return ESP_OK;
}

// Fonction d'enregistrement publique de l'API critique
esp_err_t ws_register_critical_api(httpd_handle_t server)
{
    httpd_uri_t uri_get = {
        .uri = "/api/thermostat/status",
        .method = HTTP_GET,
        .handler = critical_status_handler,
        .user_ctx = NULL};

    httpd_uri_t uri_post = {
        .uri = "/api/thermostat/config/critical",
        .method = HTTP_POST,
        .handler = critical_config_post_handler,
        .user_ctx = NULL};

    esp_err_t err = httpd_register_uri_handler(server, &uri_get);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec enregistrement GET Status Critique: %s", esp_err_to_name(err));
        return err;
    }

    err = httpd_register_uri_handler(server, &uri_post);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec enregistrement POST Config Critique: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "API Mode Critique enregistrée (GET /api/thermostat/status + POST /api/thermostat/config/critical)");
    return ESP_OK;
}
