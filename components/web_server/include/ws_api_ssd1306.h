#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enregistre l'API OLED dans le serveur HTTP
 */
esp_err_t ws_register_ssd1306_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
