#ifndef WS_API_LED_H
#define WS_API_LED_H

#include "esp_http_server.h"

/**
 * @brief Enregistre les points de terminaison (URIs) de l'API LED sur le serveur.
 * 
 * @param server Handle du serveur HTTP sur lequel enregistrer les routes.
 * @return 
 *      - ESP_OK en cas de succès.
 *      - ESP_FAIL si l'enregistrement d'une route échoue.
 */
esp_err_t ws_register_led_api(httpd_handle_t server);

#endif // WS_API_LED_H
