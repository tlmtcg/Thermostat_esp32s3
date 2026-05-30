#include "dht_task.h"

#include <stdio.h>

#include "app_context.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sd_card.h"
#include "time_utils.h"
#include "app_context.h"

// =========================================================================
// CONFIGURATION : METTRE À 0 SI LE CAPTEUR DHT N'EST PAS CÂBLÉ PHYSIQUEMENT
// =========================================================================
#define USE_DHT_SENSOR      0

static const char *TAG = "DHT_TASK";

#if USE_DHT_SENSOR
#include "dht.h"

#define DHT_GPIO_PIN        GPIO_NUM_7          // À adapter selon votre câblage
#define DHT_SENSOR_TYPE     DHT_TYPE_DHT22      // DHT_TYPE_DHT11 ou DHT_TYPE_DHT22

#define DHT_LOG_FILE_PATH   MOUNT_POINT "/dht_data.csv"
#define LOG_INTERVAL_MS     (5 * 60 * 1000)     // Log toutes les 5 minutes

static int64_t last_log_time = 0;
#endif

void dht_task(void *pvParameters)
{
#if !USE_DHT_SENSOR
    // Sécurité : Si le capteur n'est pas câblé, on nettoie le contexte global et on détruit la tâche
    // g_ctx.temperature = 0.0f;
    // g_ctx.humidity = 0.0f;
    ESP_LOGW(TAG, "Capteur DHT désactivé via flag -> Auto-destruction de la tâche.");
    vTaskDelete(NULL);
#else
    dht_task_config_t *task_config = (dht_task_config_t *)pvParameters;
    
    // Configuration de la broche GPIO pour le DHT (Entrée avec Pull-up car protocole Open-Drain)
    gpio_reset_pin(DHT_GPIO_PIN);

    while (1)
    {
        // Attente du bit d'activation de la tâche
        xEventGroupWaitBits(task_config->event_group, task_config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        float temperature = 0.0f;
        float humidity = 0.0f;
        
        // Lecture physique du capteur
        esp_err_t ret = dht_read_data(DHT_GPIO_PIN, DHT_SENSOR_TYPE, &humidity, &temperature);

        if (ret == ESP_OK)
        {
            // Mise à jour de votre contexte d'application global (Exemple de nommage à adapter)
            g_ctx.temperature = temperature;
            g_ctx.humidity = humidity;

            ESP_LOGI(TAG, "DHT: %.1f C, %.1f%%", temperature, humidity);

            // Gestion de l'écriture sur la carte SD
            int64_t now = time_utils_get_timestamp();
            if ((now - last_log_time) >= (LOG_INTERVAL_MS * 1000))
            {
                last_log_time = now;

                // On vérifie si la carte SD est disponible avant d'écrire
                char time_str[24];
                char log_buffer[128];

                time_utils_get_time_str(time_str, sizeof(time_str));
                snprintf(log_buffer, sizeof(log_buffer), "%s,%.1f,%.1f\n", time_str, temperature, humidity);

                if (sd_write_file(DHT_LOG_FILE_PATH, log_buffer,"a") != ESP_OK)
                {
                    ESP_LOGE(TAG, "Erreur ecriture log DHT");
                }
            }
        }
        else
        {
            ESP_LOGW(TAG, "Erreur lecture DHT: %s", esp_err_to_name(ret));
        }

        // Récupération du délai dynamique
        uint32_t delay_ms = *task_config->delay_ms;
        if (delay_ms < 2000) {
            delay_ms = 2000; // Sécurité : Pas de lecture DHT sous les 2 secondes (硬件限制)
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
#endif
}
