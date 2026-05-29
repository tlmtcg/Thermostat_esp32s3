#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t ws_register_history_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
