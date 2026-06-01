// #include "dht_task.h"

// #include <stdio.h>

// #include "app_context.h"
// #include "esp_log.h"
// #include "freertos/task.h"
// #include "sd_card.h"
// #include "time_utils.h"
// #include "app_context.h"
// #include <math.h>

// // =========================================================================
// // CONFIGURATION : METTRE À 0 SI LE CAPTEUR DHT N'EST PAS CÂBLÉ PHYSIQUEMENT
// // =========================================================================
// #define USE_DHT_SENSOR      0

// static const char *TAG = "DHT_TASK";

// #if !USE_DHT_SENSOR
// #include "dht.h"

// #define DHT_GPIO_PIN        GPIO_NUM_7          // À adapter selon votre câblage
// #define DHT_SENSOR_TYPE     DHT_TYPE_DHT11      // DHT_TYPE_DHT11 ou DHT_TYPE_DHT22

// #define DHT_LOG_FILE_PATH   MOUNT_POINT "/dht_data.csv"
// #define LOG_INTERVAL_MS     (5 * 60 * 1000)     // Log toutes les 5 minutes

// static int64_t last_log_time = 0;
// #endif

// void dht_task(void *pvParameters)
// {
// #if USE_DHT_SENSOR
//     ESP_LOGW(TAG, "Capteur DHT désactivé via flag -> Auto-destruction de la tâche.");
//     vTaskDelete(NULL);
// #else
//     dht_task_config_t *task_config = (dht_task_config_t *)pvParameters;

//     // Dernière valeur valide (fallback)
//     float last_temp = NAN;
//     float last_hum  = NAN;

//     gpio_reset_pin(DHT_GPIO_PIN);
//     ESP_LOGI(TAG, "DHT11 initialisé sur GPIO %d", DHT_GPIO_PIN);

//     while (1)
//     {
//         // Attente du bit d’activation
//         xEventGroupWaitBits(task_config->event_group,
//                             task_config->event_bit,
//                             pdFALSE, pdTRUE,
//                             portMAX_DELAY);

//         float temperature = NAN;
//         float humidity    = NAN;
//         esp_err_t ret     = ESP_FAIL;

//         // --- Tentatives multiples ---
//         for (int attempt = 1; attempt <= 3; attempt++)
//         {
//             ret = dht_read_data(DHT_GPIO_PIN, DHT_SENSOR_TYPE, &humidity, &temperature);
//             if (ret == ESP_OK)
//                 break;

//             ESP_LOGW(TAG, "Tentative %d/3 échouée (err=%s)",
//                      attempt, esp_err_to_name(ret));
//             vTaskDelay(pdMS_TO_TICKS(150 * attempt)); // backoff progressif
//         }

//         bool ok = false;

//         if (ret == ESP_OK &&
//             temperature > -10 && temperature < 60 &&
//             humidity >= 0 && humidity <= 100)
//         {
//             last_temp = temperature;
//             last_hum  = humidity;
//             ok = true;

//             ESP_LOGI(TAG, "DHT: %.1f C, %.1f%%", temperature, humidity);
//         }
//         else
//         {
//             ESP_LOGW(TAG, "Erreur lecture DHT: %s, fallback utilisé",
//                      ret == ESP_ERR_TIMEOUT ? "TIMEOUT" : esp_err_to_name(ret));
//         }

//         // Valeurs publiées (fallback si nécessaire)
//         float out_temp = isnan(last_temp) ? 0.0f : last_temp;
//         float out_hum  = isnan(last_hum)  ? 0.0f : last_hum;

//         // Mise à jour du contexte global
//         g_ctx.temperature = out_temp;
//         g_ctx.humidity    = out_hum;

//         // Log SD uniquement si on a une lecture plausible (ou un fallback déjà valide)
//         if (!isnan(last_temp) && !isnan(last_hum))
//         {
//             int64_t now = time_utils_get_timestamp();
//             if ((now - last_log_time) >= (LOG_INTERVAL_MS * 1000))
//             {
//                 last_log_time = now;

//                 char time_str[24];
//                 char log_buffer[128];

//                 time_utils_get_time_str(time_str, sizeof(time_str));
//                 snprintf(log_buffer, sizeof(log_buffer),
//                          "%s,%.1f,%.1f\n", time_str, out_temp, out_hum);

//                 if (sd_write_file(DHT_LOG_FILE_PATH, log_buffer, "a") != ESP_OK)
//                 {
//                     ESP_LOGE(TAG, "Erreur ecriture log DHT");
//                 }
//             }
//         }

//         // Délai dynamique avec sécurité
//         uint32_t delay_ms = *task_config->delay_ms;
//         if (delay_ms < 2000)
//             delay_ms = 2000;

//         vTaskDelay(pdMS_TO_TICKS(delay_ms));
//     }
// #endif
// }


#include "dht_task.h"

#include <stdio.h>
#include <math.h>

#include "app_context.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"    // Requis pour gpio_reset_pin et GPIO_NUM_7
#include "sd_card.h"
#include "time_utils.h"

// =========================================================================
// CONFIGURATION : METTRE À 1 SI LE CAPTEUR DHT EST CÂBLÉ PHYSIQUEMENT
// =========================================================================
#define USE_DHT_SENSOR      1

static const char *TAG = "DHT_TASK";

#if USE_DHT_SENSOR
#include "dht.h"

#define DHT_GPIO_PIN        GPIO_NUM_7          // À adapter selon votre câblage
#define DHT_SENSOR_TYPE     DHT_TYPE_DHT11      // DHT_TYPE_DHT11 ou DHT_TYPE_DHT22

#define DHT_LOG_FILE_PATH   MOUNT_POINT "/dht_data.csv"
#define LOG_INTERVAL_MS     (5 * 60 * 1000)     // Log toutes les 5 minutes

static int64_t last_log_time = 0;
#endif

void dht_task(void *pvParameters)
{
#if !USE_DHT_SENSOR
    ESP_LOGW(TAG, "Capteur DHT désactivé via flag -> Auto-destruction de la tâche.");
    vTaskDelete(NULL);
#else
    // Sécurité : vérification des paramètres de la tâche
    if (pvParameters == NULL) {
        ESP_LOGE(TAG, "Paramètres de tâche NULL -> Auto-destruction.");
        vTaskDelete(NULL);
    }

    dht_task_config_t *task_config = (dht_task_config_t *)pvParameters;

    // Dernière valeur valide (fallback)
    float last_temp = NAN;
    float last_hum  = NAN;

    gpio_reset_pin(DHT_GPIO_PIN);
    ESP_LOGI(TAG, "DHT11 initialisé sur GPIO %d", DHT_GPIO_PIN);

    while (1)
    {
        // Attente du bit d’activation
        xEventGroupWaitBits(task_config->event_group,
                            task_config->event_bit,
                            pdFALSE, pdTRUE,
                            portMAX_DELAY);

        float temperature = NAN;
        float humidity    = NAN;
        esp_err_t ret     = ESP_FAIL;

        // --- Tentatives multiples ---
        for (int attempt = 1; attempt <= 3; attempt++)
        {
            ret = dht_read_data(DHT_GPIO_PIN, DHT_SENSOR_TYPE, &humidity, &temperature);
            if (ret == ESP_OK)
                break;

            ESP_LOGW(TAG, "Tentative %d/3 échouée (err=%s)",
                     attempt, esp_err_to_name(ret));
            
            // CORRECTION : Augmentation du délai à 1.5s. Le DHT a besoin de temps 
            // pour s'en remettre après un timeout ou une lecture ratée.
            vTaskDelay(pdMS_TO_TICKS(1500)); 
        }

        if (ret == ESP_OK &&
            temperature > -10 && temperature < 60 &&
            humidity >= 0 && humidity <= 100)
        {
            last_temp = temperature;
            last_hum  = humidity;

            ESP_LOGI(TAG, "DHT: %.1f C, %.1f%%", temperature, humidity);
        }
        else
        {
            ESP_LOGW(TAG, "Erreur lecture DHT: %s, fallback utilisé",
                     ret == ESP_ERR_TIMEOUT ? "TIMEOUT" : esp_err_to_name(ret));
        }

        // Valeurs publiées (fallback si nécessaire)
        float out_temp = isnan(last_temp) ? 0.0f : last_temp;
        float out_hum  = isnan(last_hum)  ? 0.0f : last_hum;

        // Mise à jour du contexte global
        g_ctx.temperature = out_temp;
        g_ctx.humidity    = out_hum;

        // Log SD uniquement si on a une lecture plausible d'enregistrée
        if (!isnan(last_temp) && !isnan(last_hum))
        {
            int64_t now = time_utils_get_timestamp();
            if ((now - last_log_time) >= LOG_INTERVAL_MS)
            {
                last_log_time = now;

                char time_str[24];
                char log_buffer[128];

                time_utils_get_time_str(time_str, sizeof(time_str));
                snprintf(log_buffer, sizeof(log_buffer),
                         "%s,%.1f,%.1f\n", time_str, out_temp, out_hum);

                if (sd_write_file(DHT_LOG_FILE_PATH, log_buffer, "a") != ESP_OK)
                {
                    ESP_LOGE(TAG, "Erreur ecriture log DHT");
                }
            }
        }

        // Délai dynamique avec sécurité anti-plantage
        uint32_t delay_ms = (task_config->delay_ms != NULL) ? *task_config->delay_ms : 2000;
        if (delay_ms < 2000) {
            delay_ms = 2000; // Spécification matérielle du DHT : max 1 lecture toutes les 2s
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
#endif
}