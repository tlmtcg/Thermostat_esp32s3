#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Enregistre les endpoints GET/POST pour /api/config
     *
     * @param server Handle du serveur HTTP
     * @return esp_err_t ESP_OK si OK, erreur sinon
     */
    esp_err_t ws_register_config_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
