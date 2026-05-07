#pragma once
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ws_register_logs_api(httpd_handle_t server);
void init_web_log_capture(void);

#ifdef __cplusplus
}
#endif
