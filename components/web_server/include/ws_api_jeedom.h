#pragma once

#include <esp_http_server.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enregistre les endpoints d'API Jeedom (GET et POST /api/jeedom)
 * @param server Le handle du serveur HTTP actif
 * @return ESP_OK en cas de succès
 */
esp_err_t register_jeedom_web_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif