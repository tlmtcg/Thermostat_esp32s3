#pragma once

#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scan WiFi et retourne un tableau JSON
 */
cJSON *wifi_service_scan(void);

/**
 * @brief Retourne l'état WiFi (STA + AP) sous forme JSON
 */
cJSON *wifi_service_status(void);

/**
 * @brief Tente une connexion WiFi via le wifi_manager
 */
esp_err_t wifi_service_connect(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif
