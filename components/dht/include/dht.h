#ifndef DHT_H
#define DHT_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DHT_TYPE_DHT11,
    DHT_TYPE_DHT22
} dht_type_t;

/**
 * @brief Lit la température et l'humidité depuis un capteur DHT11 ou DHT22.
 * 
 * @param gpio_num Le numéro de PIN GPIO utilisé (Data)
 * @param type Le type de capteur (DHT_TYPE_DHT11 ou DHT_TYPE_DHT22)
 * @param humidity Pointeur pour stocker l'humidité relative (%)
 * @param temperature Pointeur pour stocker la température (°C)
 * @return esp_err_t ESP_OK en cas de succès, ESP_ERR_TIMEOUT ou ESP_ERR_INVALID_CRC en cas d'échec
 */
esp_err_t dht_read_data(gpio_num_t gpio_num, dht_type_t type, float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif

#endif // DHT_H
