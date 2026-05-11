#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "cJSON.h"

// Déclarer la structure i2c_scan_result_t AVANT son utilisation
typedef struct {
    uint8_t *addresses;  // Tableau des adresses I2C
    size_t count;        // Nombre d'adresses trouvées
    char timestamp[20];  // Timestamp au format "JJ/MM/AAAA HH:MM:SS"
} i2c_scan_result_t;

// Initialisation du bus I2C
esp_err_t i2c_manager_init(void);

// Scan des périphériques I2C
esp_err_t i2c_manager_scan(void);

// Récupère le JSON des périphériques (pour l'API)
cJSON *i2c_manager_get_devices_json(void);

// Récupère le résultat brut du scan
const i2c_scan_result_t *i2c_manager_get_scan_result(void);

// Reconfigure le bus I2C avec de nouveaux paramètres
esp_err_t i2c_manager_reinit(int sda_gpio, int scl_gpio, int freq_hz);

