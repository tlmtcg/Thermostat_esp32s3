#pragma once
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ws_register_static(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
