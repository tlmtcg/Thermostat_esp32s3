#pragma once
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tasks_get_handler(httpd_req_t *req) ;
esp_err_t tasks_post_handler(httpd_req_t *req) ;
esp_err_t ws_register_tasks_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
