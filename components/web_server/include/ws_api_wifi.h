#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

/**
 * @brief Enregistre toutes les routes API WiFi :
 *
 *   GET  /api/wifi          → état réseau (RSSI, IP, clients, auth…)
 *   GET  /api/wifi/config   → configuration WiFi (g_wifi_cfg)
 *   POST /api/wifi          → mise à jour config + sauvegarde NVS
 *   GET  /api/wifi/scan     → scan des réseaux WiFi
 *   POST /api/wifi/connect  → test de connexion STA
 *
 * @param server  Handle du serveur HTTPD
 * @return ESP_OK si tout est enregistré correctement
 */
esp_err_t ws_register_wifi_api(httpd_handle_t server);
