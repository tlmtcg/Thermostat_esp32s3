#ifndef WS_API_PROGRAM_H
#define WS_API_PROGRAM_H

#include "esp_http_server.h"
#include "esp_err.h"

/**
 * @brief Enregistre les routes de l'API de programmation du chauffage
 *        Routes : /api/program/get, /api/program/set, /api/program/reset
 * 
 * @param server Le handle du serveur HTTP actif
 * @return esp_err_t ESP_OK en cas de succès
 */
esp_err_t ws_register_program_api(httpd_handle_t server);

#endif // WS_API_PROGRAM_H
