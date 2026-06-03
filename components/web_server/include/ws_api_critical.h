#ifndef WS_API_CRITICAL_H
#define WS_API_CRITICAL_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enregistre les endpoints de l'API du Mode Critique auprès du serveur HTTP.
 * * Expose les routes suivantes :
 * - GET  /api/thermostat/status           : Récupère l'état des capteurs et de la config de secours
 * - POST /api/thermostat/config/critical  : Met à jour les paramètres temporels du PWM de secours
 * * @param server Handle du serveur HTTP actif
 * @return ESP_OK si l'enregistrement réussit, ou un code d'erreur ESP-IDF
 */
esp_err_t ws_register_critical_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // WS_API_CRITICAL_H