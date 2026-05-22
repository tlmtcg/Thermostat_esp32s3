#include "dht.h"
#include "rom/ets_sys.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "dht_task.h"

static const char *TAG = "DHT";

// Attendre que la broche change d'état avec un timeout en microsecondes
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

esp_err_t dht_read_data(gpio_num_t gpio_num, dht_type_t type, float *humidity, float *temperature)
{
    uint8_t data[5] = {0};
    uint32_t duration = 0;

    // 1. Signal de Start envoyé par l'ESP32
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT_OD); // Open-Drain avec pull-up externe requise
    gpio_set_level(gpio_num, 0);
    
    // Le DHT11 demande au moins 18ms, le DHT22 demande au moins 1ms
    ets_delay_us(type == DHT_TYPE_DHT11 ? 20000 : 2000);
    
    gpio_set_level(gpio_num, 1);
    ets_delay_us(40); // Attente de la réponse du capteur

    // 2. Passer la broche en entrée pour lire la réponse
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);

    // Réponse du DHT : L'état bas dure 80us, puis le haut dure 80us
    if (dht_wait_level(gpio_num, 80, 1, NULL) != ESP_OK) return ESP_ERR_TIMEOUT; // Attente fin du 1 initial
    if (dht_wait_level(gpio_num, 90, 0, NULL) != ESP_OK) return ESP_ERR_TIMEOUT; // Attente de la mise à bas
    if (dht_wait_level(gpio_num, 90, 1, NULL) != ESP_OK) return ESP_ERR_TIMEOUT; // Attente de la mise à haut

    // 3. Lecture des 40 bits (5 octets) de données
    for (int i = 0; i < 40; i++) {
        // Chaque bit commence par un état bas de 50us
        if (dht_wait_level(gpio_num, 60, 0, NULL) != ESP_OK) return ESP_ERR_TIMEOUT;

        // L'état haut qui suit détermine la valeur du bit : 
        // ~26-28us = '0' | ~70us = '1'
        if (dht_wait_level(gpio_num, 80, 1, &duration) != ESP_OK) return ESP_ERR_TIMEOUT;

        data[i / 8] <<= 1;
        if (duration > 40) { // Si l'état haut a duré plus de 40us, c'est un '1'
            data[i / 8] |= 1;
        }
    }

    // 4. Vérification du Checksum (Somme de contrôle)
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        ESP_LOGE(TAG, "Erreur de Checksum (CRC)");
        return ESP_ERR_INVALID_CRC;
    }

    // 5. Conversion des données brutes selon le modèle
    if (type == DHT_TYPE_DHT11) {
        *humidity = (float)data[0];
        *temperature = (float)data[2];
        // Prise en compte de la partie décimale si supportée par certains clones de DHT11
        if (data[1] < 10) *humidity += (float)data[1] * 0.1f;
        if (data[3] < 10) *temperature += (float)data[3] * 0.1f;
    } else { // DHT22 / AM2302
        float h = (float)((data[0] << 8) | data[1]) * 0.1f;
        float t = (float)((data[2] & 0x7F) << 8 | data[3]) * 0.1f;
        if (data[2] & 0x80) { // Bit de signe pour les températures négatives
            t = -t;
        }
        *humidity = h;
        *temperature = t;
    }

    return ESP_OK;
}
