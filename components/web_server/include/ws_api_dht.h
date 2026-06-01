#ifndef WS_API_DHT_H
#define WS_API_DHT_H

#include "esp_http_server.h"
#include "esp_err.h"

/**
 * @brief Enregistre les URI de l'API REST pour le capteur DHT (GET et POST)
 * @param server Handle du serveur HTTP actif
 * @return ESP_OK en cas de succès, ou un code d'erreur ESP-IDF
 */
esp_err_t ws_register_dht_api(httpd_handle_t server);

#endif // WS_API_DHT_H
