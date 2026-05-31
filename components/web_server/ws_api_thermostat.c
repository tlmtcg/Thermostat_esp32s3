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
#include "thermostat.h"

// Tag utilisé pour les logs ESP-IDF associés à ce module
static const char *TAG = "WS_THERMOSTAT";

/* =========================================================
 * STATUS : Gestion de la récupération de l'état du système
 * ========================================================= */
/**
 * @brief Gestionnaire HTTP pour renvoyer le statut complet du thermostat au format JSON
 * @param req Pointeur vers la structure de requête HTTP d'ESP-IDF
 * @return ESP_OK en cas de succès, ou un code d'erreur HTTPD
 */
static esp_err_t index_status_handler(httpd_req_t *req)
{
    // 1. Récupération directe de la chaîne JSON générée par le composant thermostat.
    // Cette chaîne contient déjà toutes les anciennes et nouvelles variables (temp_ext, humidité, prévisions).
    char *json_response = thermostat_get_json_status();

    if (json_response == NULL) {
        ESP_LOGE(TAG, "Échec de la génération du statut JSON");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 2. Configuration de l'en-tête HTTP pour indiquer au navigateur qu'il s'agit de JSON
    httpd_resp_set_type(req, "application/json");

    // 3. Envoi de la chaîne JSON au client
    esp_err_t err = httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);

    // 4. LIBÉRATION CRUCIALE DE LA MÉMOIRE
    // La chaîne renvoyée par cJSON doit impérativement être libérée avec free()
    free(json_response);

    return err;
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
    else if (strcmp(buf, "LEARNING") == 0)
    {
        thermostat_set_mode(THERMOSTAT_MODE_LEARNING);
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
