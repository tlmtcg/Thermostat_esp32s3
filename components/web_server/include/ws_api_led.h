/**
 * @file ws_api_led.h
 * @brief Déclaration de l'API Web pour le contrôle des LED.
 */

#pragma once

#include <esp_http_server.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enregistre tous les handlers de l'API LED sur le serveur HTTP fourni.
 * 
 * Routes enregistrées :
 * - GET  /api/led          : Liste les ambiances (infos) et alarmes en mémoire.
 * - POST /api/led/simulate : Lance une simulation par index.
 * - POST /api/led/off      : Éteint la LED et efface les alarmes.
 * - POST /api/led/add      : Ajoute une nouvelle entrée à la base de données.
 * - POST /api/led/delete   : Supprime une entrée par son nom.
 * - POST /api/led/preview  : Change la couleur en temps réel (aperçu).
 * 
 * @param server Handle du serveur HTTPD déjà initialisé.
 */
esp_err_t ws_register_led_api(httpd_handle_t server);

/**
 * @brief Vérifie si un nom existe déjà dans la base de données (infos ou alarmes).
 * 
 * @param name Nom à vérifier.
 * @return true si le nom existe, false sinon.
 */
bool name_exists(const char *name);

#ifdef __cplusplus
}
#endif
