#pragma once
#include "esp_http_server.h"

typedef void (*ws_register_fn_t)(httpd_handle_t server);

/**
 * @brief Enregistre toutes les routes HTTP (APIs + fichiers statiques)
 */
void ws_register_all(httpd_handle_t server);
