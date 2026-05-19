#include "ws_api_thermostat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_http_server.h"

#include "thermostat.h"
#include "heating_program.h"
#include "relay.h"
#include "sht31.h"
#include "time.h"

// Tag utilisé pour les logs ESP-IDF associés à ce module
static const char *TAG = "WS_THERMOSTAT";

/* =========================================================
 * STATUS : Gestion de la récupération de l'état du système
 * ========================================================= */

/**
 * @brief Handler HTTP GET pour récupérer l'état actuel du thermostat en JSON.
 * 
 * @param req Pointeur vers la requête HTTP entrante.
 * @return esp_err_t ESP_OK en cas de succès, ou code d'erreur ESP-IDF.
 */
static esp_err_t index_status_handler(httpd_req_t *req)
{
    // Récupération des états internes du système
    const chauffage_config_t *cfg = heating_get_config();
    thermostat_state_t st = thermostat_get_state();

    float current_setpoint = heating_get_temp_current(); // consigne calculée
    bool relay_state = get_relay_state();

    // ⚠️ récupérer UNE seule fois le capteur
    const sht31_state_t *env = sht31_get_state();

    float temperature = env ? env->temperature : -127.0f;
    float humidity    = env ? env->humidity : -1.0f;
    bool valid        = env ? env->valid : false;
    time_t last_update = env ? env->last_update : 0;

    char json[384]; // ⚠️ augmenté

    snprintf(json, sizeof(json),
             "{"
             "\"temperature\":%.2f,"
             "\"humidity\":%.2f,"
             "\"valid\":%s,"
             "\"last_update\":%ld,"
             "\"target\":%.2f,"
             "\"relay\":%s,"
             "\"mode\":%d,"
             "\"enabled\":%s"
             "}",
             temperature,
             humidity,
             valid ? "true" : "false",
             (long)last_update,
             current_setpoint,
             relay_state ? "true" : "false",
             st.mode,
             st.enabled ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* =========================================================
 * COMMAND : Gestion des ordres reçus
 * ========================================================= */

/**
 * @brief Handler HTTP POST pour traiter les commandes textuelles envoyées au thermostat.
 * 
 * @param req Pointeur vers la requête HTTP entrante contenant le payload de commande.
 * @return esp_err_t ESP_OK en cas de succès, ESP_FAIL en cas d'erreur de réception.
 */
static esp_err_t index_command_handler(httpd_req_t *req)
{
    char buf[128];

    // Lecture du corps (body) de la requête POST
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        // Gestion de l'erreur si le corps est vide ou illisible
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }

    // Sécurisation de la chaîne de caractères reçue
    buf[len] = '\0';
    ESP_LOGI(TAG, "CMD: %s", buf);

    /* -------- MODE : Changement du mode de fonctionnement -------- */
    if (strcmp(buf, "AUTO") == 0)
    {
        thermostat_set_mode(THERMOSTAT_MODE_AUTO);
    }
    else if (strcmp(buf, "MANUAL") == 0)
    {
        thermostat_set_mode(THERMOSTAT_MODE_MANUAL);
    }
    else if (strcmp(buf, "ABSENT") == 0)
    {
        thermostat_set_mode(THERMOSTAT_MODE_ABSENT);
    }
    else if (strcmp(buf, "HORS_GEL") == 0)
    {
        thermostat_set_mode(THERMOSTAT_MODE_HORS_GEL);
    }

    /* -------- ENABLE : Activation / Désactivation globale -------- */
    else if (strcmp(buf, "ON") == 0)
    {
        thermostat_set_enabled(true);
    }
    else if (strcmp(buf, "OFF") == 0)
    {
        thermostat_set_enabled(false);
    }

    /* -------- CONSIGNE : Modification de la température cible -------- */
    else if (strncmp(buf, "SET:", 4) == 0)
    {
        // Extraction et conversion de la valeur numérique après "SET:"
        float value = atof(buf + 4);

        // Validation de sécurité sur la plage de température autorisée (5°C à 35°C)
        if (value >= 5.0f && value <= 35.0f)
        {
            thermostat_set_consigne(value);
        }
    }

    // Réponse de confirmation au client
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* =========================================================
 * REGISTER : Initialisation et routage
 * ========================================================= */

/**
 * @brief Enregistre les routes de l'API Index auprès du serveur HTTP de l'ESP32.
 * 
 * @param server Handle du serveur HTTP actif.
 * @return esp_err_t ESP_OK si l'enregistrement réussit.
 */
esp_err_t ws_register_index_api(httpd_handle_t server)
{
    // Définition de l'URI pour la récupération du statut (GET)
    httpd_uri_t uri_status = {
        .uri = "/api/index/status",
        .method = HTTP_GET,
        .handler = index_status_handler,
        .user_ctx = NULL};

    // Définition de l'URI pour l'envoi de commandes (POST)
    httpd_uri_t uri_command = {
        .uri = "/api/index/cmd",
        .method = HTTP_POST,
        .handler = index_command_handler,
        .user_ctx = NULL};

    // Enregistrement des routes et vérification des erreurs d'initialisation
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_status));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_command));

    ESP_LOGI(TAG, "Index API registered");
    return ESP_OK;
}
