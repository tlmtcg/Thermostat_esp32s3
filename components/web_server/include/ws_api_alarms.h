#ifndef WS_API_ALARMS_H
#define WS_API_ALARMS_H

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enregistrement des routes HTTP liées aux alarmes
esp_err_t ws_register_alarms_api(httpd_handle_t server);

// Handlers HTTP
esp_err_t get_active_alarms_handler(httpd_req_t *req);
esp_err_t get_history_handler(httpd_req_t *req);
esp_err_t clear_alarms_handler(httpd_req_t *req);
esp_err_t delete_alarm_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // WS_API_ALARMS_H
