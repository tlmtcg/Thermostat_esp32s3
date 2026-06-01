#ifndef DHT_H
#define DHT_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include <time.h>

// Types de capteurs gérés
typedef enum {
    DHT_TYPE_DHT11,
    DHT_TYPE_DHT22
} dht_sensor_type_t;

// Structure de configuration (Style SHT31)
typedef struct {
    gpio_num_t gpio_pin;
    dht_sensor_type_t sensor_type;
    uint32_t read_interval_ms;
    bool log_to_sd;
} dht_config_t;

// Structure de Runtime pour le statut global
typedef struct {
    float temperature;
    float humidity;
    bool valid;
    bool initialized;
    bool running;
    uint32_t read_count;
    uint32_t error_count;
    uint32_t consecutive_error_count;
    esp_err_t last_error_code;
    time_t last_error_at;
    time_t last_success_at;
    time_t last_update;
    char last_error[32];
} dht_runtime_t;

// Prototype du protocole physique (Bas niveau)
esp_err_t dht_read_data(gpio_num_t gpio_num, dht_sensor_type_t type, float *humidity, float *temperature);

// Prototypes de gestion applicative (Haut niveau)
esp_err_t dht_init(gpio_num_t gpio_pin, dht_sensor_type_t type);
void dht_deinit(void);
esp_err_t dht_start(gpio_num_t gpio_pin, dht_sensor_type_t type);
void dht_stop(void);
esp_err_t dht_read_sample(float *temp, float *hum);
esp_err_t dht_get_config(dht_config_t *out);
esp_err_t dht_set_config(const dht_config_t *config);
const dht_runtime_t *dht_get_runtime(void);
char *dht_get_json_status(void);

#endif // DHT_H
