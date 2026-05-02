#ifndef LED_SERVICE_H
#define LED_SERVICE_H

#include "cJSON.h"
#include "led_types.h" // Indispensable pour que le compilateur connaisse les types

/**
 * @brief Génère l'état complet de la DB LED en JSON pour le client Web
 */
cJSON* led_db_get_json_status(void);

#endif // LED_SERVICE_H
