#ifndef WS_API_SD_H
#define WS_API_SD_H

#include "esp_http_server.h"

// Enregistre les routes /api/sd/list et /api/sd/read
esp_err_t ws_register_sd_api(httpd_handle_t server);
esp_err_t sd_create_dir(const char *path);
esp_err_t sd_remove_dir(const char *path);

#endif