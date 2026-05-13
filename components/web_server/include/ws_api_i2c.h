#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  REGISTER API                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Enregistre toutes les routes HTTP I2C
 *
 * Routes :
 *  - POST /api/i2c/scan
 *  - GET  /api/i2c/devices
 *  - GET  /api/i2c/config
 *  - POST /api/i2c/config/set
 */
esp_err_t ws_register_i2c_api(httpd_handle_t server);

/* -------------------------------------------------------------------------- */
/*  CONFIG SAVE                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Sauvegarde la configuration I2C dans le stockage (SD / flash)
 *
 * Utilisé après modification via API HTTP.
 */
void i2c_save_config_to_sdcard(void);

#ifdef __cplusplus
}
#endif
