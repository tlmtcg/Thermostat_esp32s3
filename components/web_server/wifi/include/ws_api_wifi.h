#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

/**
 * @brief Enregistre toutes les routes HTTP liées au WiFi.
 *
 * Routes :
 *   GET  /api/wifi
 *   GET  /api/wifi/scan
 *   POST /api/wifi/connect
 *
 * @param server Handle du serveur HTTP
 * @return ESP_OK si tout est enregistré correctement
 */
esp_err_t ws_register_wifi_api(httpd_handle_t server);
