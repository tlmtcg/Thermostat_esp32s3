#ifndef WS_API_LED_H
#define WS_API_LED_H

#include <esp_http_server.h>

/**
 * @brief Enregistre les points d'accès API pour le contrôle des LED
 * @param server Handle du serveur HTTP
 */
void ws_register_led_api(httpd_handle_t server);

#endif // WS_API_LED_H
