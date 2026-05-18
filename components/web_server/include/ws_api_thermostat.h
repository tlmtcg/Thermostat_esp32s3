#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enregistre les endpoints API de la page principale
 *
 * Endpoints:
 *   GET  /api/index/status
 *   POST /api/index/cmd
 */
esp_err_t ws_register_index_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif