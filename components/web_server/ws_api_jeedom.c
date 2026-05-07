#include "ws_api_jeedom.h"
#include "jeedom.h"

esp_err_t register_jeedom_web_api(httpd_handle_t server) {
    // Définition des URIs (GET et POST)
    static const httpd_uri_t jeedom_get_uri = {
        .uri      = "/api/jeedom",
        .method   = HTTP_GET,
        .handler  = get_jeedom_config_handler,
        .user_ctx = NULL
    };

    static const httpd_uri_t jeedom_post_uri = {
        .uri      = "/api/jeedom",
        .method   = HTTP_POST,
        .handler  = post_jeedom_config_handler,
        .user_ctx = NULL
    };

    // Enregistrement
    httpd_register_uri_handler(server, &jeedom_get_uri);
    httpd_register_uri_handler(server, &jeedom_post_uri);

    return ESP_OK;
}
