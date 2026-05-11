#pragma once

#include <esp_http_server.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fichier de configuration général
#define CONFIG_FILE "/sdcard/config.json"

// Enregistre les routes API pour l'I2C
esp_err_t ws_register_i2c_api(httpd_handle_t server);

void i2c_save_config_to_sdcard(void);

// Reconfigure le bus I2C avec de nouveaux paramètres
esp_err_t i2c_manager_reinit(int sda_gpio, int scl_gpio, int freq_hz);
#ifdef __cplusplus
}
#endif