#include "dht.h"
#include "rom/ets_sys.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DHT_DRIVER";

// Variables globales internes pour maintenir l'état (Style SHT31)
static dht_config_t active_config = {
    .gpio_pin = GPIO_NUM_7,
    .sensor_type = DHT_TYPE_DHT11,
    .read_interval_ms = 2000,
    .log_to_sd = false
};

static dht_runtime_t active_runtime = {
    .temperature = 0.0f,
    .humidity = 0.0f,
    .valid = false,
    .initialized = true,
    .running = true,
    .read_count = 0,
    .error_count = 0,
    .consecutive_error_count = 0,
    .last_error_code = ESP_OK,
    .last_error_at = 0,
    .last_success_at = 0,
    .last_update = 0,
    .last_error = "Aucun"
};

// Attente de changement d'état d'une broche avec timeout microsecondes
static inline esp_err_t dht_wait_level(gpio_num_t gpio_num, uint32_t timeout_us, uint32_t level, uint32_t *duration)
{
    uint64_t start = esp_timer_get_time();
    while (gpio_get_level(gpio_num) == level) {
        if ((esp_timer_get_time() - start) > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
    }
    if (duration) {
        *duration = (uint32_t)(esp_timer_get_time() - start);
    }
    return ESP_OK;
}

// Lecture brute du protocole Single-Wire du DHT
esp_err_t dht_read_data(gpio_num_t gpio_num, dht_sensor_type_t type, float *humidity, float *temperature)
{
    uint8_t data[5] = {0};
    uint32_t duration = 0;

    // 1. Signal de Start généré par l'ESP32
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT_OD); 
    gpio_set_level(gpio_num, 0);
    
    ets_delay_us(type == DHT_TYPE_DHT11 ? 20000 : 2000);
    
    gpio_set_level(gpio_num, 1);
    ets_delay_us(40); 

    // 2. Commutation de la broche en entrée
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);

    // Poignée de main (Handshake) du DHT
    if (dht_wait_level(gpio_num, 80, 1, NULL) != ESP_OK) return ESP_ERR_TIMEOUT; 
    if (dht_wait_level(gpio_num, 90, 0, NULL) != ESP_OK) return ESP_ERR_TIMEOUT; 
    if (dht_wait_level(gpio_num, 90, 1, NULL) != ESP_OK) return ESP_ERR_TIMEOUT; 

    // 3. Extraction des 40 bits
    for (int i = 0; i < 40; i++) {
        if (dht_wait_level(gpio_num, 60, 0, NULL) != ESP_OK) return ESP_ERR_TIMEOUT;
        if (dht_wait_level(gpio_num, 80, 1, &duration) != ESP_OK) return ESP_ERR_TIMEOUT;

        data[i / 8] <<= 1;
        if (duration > 40) { 
            data[i / 8] |= 1;
        }
    }

    // 4. Validation du Checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        return ESP_ERR_INVALID_CRC;
    }

    // 5. Interprétation des grandeurs physiques
    if (type == DHT_TYPE_DHT11) {
        *humidity = (float)data[0];
        *temperature = (float)data[2];
        if (data[1] < 10) *humidity += (float)data[1] * 0.1f;
        if (data[3] < 10) *temperature += (float)data[3] * 0.1f;
    } else { 
        float h = (float)((data[0] << 8) | data[1]) * 0.1f;
        float t = (float)((data[2] & 0x7F) << 8 | data[3]) * 0.1f;
        if (data[2] & 0x80) t = -t; 
        *humidity = h;
        *temperature = t;
    }

    return ESP_OK;
}

// Getters et Setters pour la configuration (Appelés par l'API POST)
esp_err_t dht_get_config(dht_config_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    memcpy(out, &active_config, sizeof(dht_config_t));
    return ESP_OK;
}

esp_err_t dht_set_config(const dht_config_t *config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    memcpy(&active_config, config, sizeof(dht_config_t));
    return ESP_OK;
}

// Récupération de l'état runtime brut
const dht_runtime_t *dht_get_runtime(void) {
    return &active_runtime;
}

// Génération dynamique du JSON (Appelé par l'API GET)
char *dht_get_json_status(void) {
    char *buf = malloc(512);
    if (!buf) return NULL;

    // Synchronisation temporaire avec le timestamp actuel pour le JSON
    time_t now = time(NULL);
    active_runtime.last_update = now;

    snprintf(buf, 512,
        "{"
            "\"runtime\":{"
                "\"temperature\":%.1f,"
                "\"humidity\":%.1f,"
                "\"valid\":%s,"
                "\"initialized\":%s,"
                "\"running\":%s,"
                "\"read_count\":%lu,"
                "\"error_count\":%lu,"
                "\"consecutive_error_count\":%lu,"
                "\"last_error_code\":%d,"
                "\"last_error_at\":%lld,"
                "\"last_success_at\":%lld,"
                "\"last_update\":%lld,"
                "\"last_error\":\"%s\""
            "},"
            "\"config\":{"
                "\"gpio_pin\":%d,"
                "\"sensor_type\":%d,"
                "\"read_interval_ms\":%lu,"
                "\"log_to_sd\":%s"
            "}"
        "}",
        active_runtime.temperature, active_runtime.humidity,
        active_runtime.valid ? "true" : "false",
        active_runtime.initialized ? "true" : "false",
        active_runtime.running ? "true" : "false",
        (unsigned long)active_runtime.read_count,
        (unsigned long)active_runtime.error_count,
        (unsigned long)active_runtime.consecutive_error_count,
        active_runtime.last_error_code,
        (long long)active_runtime.last_error_at,
        (long long)active_runtime.last_success_at,
        (long long)active_runtime.last_update,
        active_runtime.last_error,
        active_config.gpio_pin, active_config.sensor_type,
        (unsigned long)active_config.read_interval_ms,
        active_config.log_to_sd ? "true" : "false"
    );

    return buf;
}
