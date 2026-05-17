#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

esp_err_t ws_register_ota_api(httpd_handle_t server);