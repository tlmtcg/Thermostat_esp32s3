#pragma once
#include "esp_err.h"
#include "cJSON.h"

cJSON *wifi_service_scan(void);
cJSON *wifi_service_status(void);
esp_err_t wifi_service_connect(const char *ssid, const char *pass);
