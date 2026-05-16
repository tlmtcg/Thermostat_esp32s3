#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enregistre l'API de contrôle de l'écran OLED U8g2 dans le serveur HTTP (via WebSocket)
 * 
 * @param server Handle du serveur HTTP actif
 * @return esp_err_t ESP_OK en cas de succès, ou un code d'erreur système
 */
esp_err_t ws_register_u8g2_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif