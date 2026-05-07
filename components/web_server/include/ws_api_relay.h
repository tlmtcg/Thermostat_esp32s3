#pragma once
#include "esp_http_server.h"


#ifdef __cplusplus
extern "C" {
#endif

esp_err_t relay_apply_config(bool force_state, uint32_t new_min_delay);
esp_err_t ws_register_relay_api(httpd_handle_t server);

#ifdef __cplusplus
}
#endif




