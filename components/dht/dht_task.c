#include "dht_task.h"
#include "dht.h"
#include <stdio.h>
#include <math.h>

#include "app_context.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sd_card.h"
#include "time_utils.h"
#include "alert_manager.h"
#include <config_runtime.h>
#include "sht31.h"
#include "thermostat.h"

// =========================================================================
// CONFIGURATION ACTIVE VIA MARQUEUR
// =========================================================================
#define USE_DHT_SENSOR 1
static const char *TAG = "DHT_TASK";

#if USE_DHT_SENSOR
#define DHT_GPIO_PIN GPIO_NUM_7
#define DHT_SENSOR_TYPE DHT_TYPE_DHT11
#define DHT_LOG_FILE_PATH MOUNT_POINT "/dht_data.csv"
#define LOG_INTERVAL_MS (5 * 60 * 1000) // 5 Minutes

// Seuil d'échecs de cycles complets avant de déclarer la panne
#define DHT_PANNE_SEUIL_CONSECUTIF 3

static int64_t last_log_time = 0;
static float last_temp = NAN;
static float last_hum = NAN;
static uint32_t consecutive_errors = 0;
#endif

/**
 * Gestion robuste des lectures avec relances automatiques et backoff temporisé
 */
esp_err_t dht_perform_measurement(float *out_temp, float *out_hum)
{
    esp_err_t ret = ESP_FAIL;

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        ret = dht_read_data(DHT_GPIO_PIN, DHT_SENSOR_TYPE, out_hum, out_temp);
        if (ret == ESP_OK)
            break;

        ESP_LOGW(TAG, "Tentative %d/3 échouée (%s)", attempt, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1500)); // Laisse le capteur recharger son bus
    }
    return ret;
}

/**
 * Tâche principale FreeRTOS synchronisée par l'Event Group
 */
void dht_task(void *pvParameters)
{
#if !USE_DHT_SENSOR
    ESP_LOGW(TAG, "Capteur DHT désactivé globalement à la compilation -> Destruction de la tâche.");
    vTaskDelete(NULL);
#else
    if (pvParameters == NULL)
    {
        ESP_LOGE(TAG, "Paramètres de tâche manquants -> Destruction.");
        vTaskDelete(NULL);
    }

    dht_task_config_t *task_config = (dht_task_config_t *)pvParameters;
    gpio_reset_pin(DHT_GPIO_PIN);
    gpio_set_pull_mode(DHT_GPIO_PIN, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "DHT initialisé avec succès sur GPIO %d avec Pull-Up", DHT_GPIO_PIN);

    while (1)
    {
        // --- 1. VERIFICATION DE LA DESACTIVATION RUNTIME (WEB) ---
        // Si l'intervalle est à 0 ou qu'un flag explicite est faux, on coupe proprement
        if (g_cfg.dht_read_int_ms == 0)
        {
            ESP_LOGW(TAG, "DHT désactivé depuis la configuration Web -> Arrêt propre de la tâche.");

            // On invalide le runtime pour l'interface web
            dht_runtime_t *runtime = (dht_runtime_t *)dht_get_runtime();
            if (runtime)
            {
                runtime->valid = false;
            }

            // Libération éventuelle du GPIO pour éviter les conflits
            gpio_reset_pin(DHT_GPIO_PIN);

            // Destruction définitive de cette tâche
            vTaskDelete(NULL);
        }

        xEventGroupWaitBits(task_config->event_group, task_config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        float current_temp = NAN;
        float current_hum = NAN;

        dht_runtime_t *runtime = (dht_runtime_t *)dht_get_runtime();
        runtime->read_count++;

        esp_err_t ret = dht_perform_measurement(&current_temp, &current_hum);

        if (ret == ESP_OK && current_temp > -15 && current_temp < 65)
        {
            last_temp = current_temp;
            last_hum = current_hum;
            consecutive_errors = 0;

            runtime->temperature = current_temp;
            runtime->humidity = current_hum;
            runtime->valid = true;
            runtime->consecutive_error_count = 0;
            runtime->last_success_at = time(NULL);

            alert_remove("Capteur DHT en panne");
            ESP_LOGI(TAG, "DHT: %.1f C, %.1f%%", last_temp, last_hum);
        }
        else
        {
            consecutive_errors++;

            runtime->valid = false;
            runtime->error_count++;
            runtime->consecutive_error_count = consecutive_errors;
            runtime->last_error_code = ret;
            runtime->last_error_at = time(NULL);

            if (ret == ESP_ERR_TIMEOUT)
            {
                strncpy(runtime->last_error, "TIMEOUT", sizeof(runtime->last_error));
            }
            else if (ret == ESP_ERR_INVALID_CRC)
            {
                strncpy(runtime->last_error, "CRC_ERROR", sizeof(runtime->last_error));
            }
            else
            {
                strncpy(runtime->last_error, "READ_FAILED", sizeof(runtime->last_error));
            }

            ESP_LOGW(TAG, "Erreur lecture DHT: %s, fallback utilisé (consécutives: %lu)",
                     runtime->last_error, (unsigned long)consecutive_errors);

            if (consecutive_errors >= DHT_PANNE_SEUIL_CONSECUTIF)
            {
                alert_add("Capteur DHT en panne");
                gpio_reset_pin(DHT_GPIO_PIN);
            }
        }

        g_ctx.temperature = isnan(last_temp) ? 0.0f : last_temp;
        g_ctx.humidity = isnan(last_hum) ? 0.0f : last_hum;

        if (!isnan(last_temp))
        {
            int64_t now = time_utils_get_timestamp();
            if ((now - last_log_time) >= (LOG_INTERVAL_MS * 1000))
            {
                last_log_time = now;
                char time_str[24];
                char log_buf[128];
                time_utils_get_time_str(time_str, sizeof(time_str));
                snprintf(log_buf, sizeof(log_buf), "%s,%.1f,%.1f\n", time_str, g_ctx.temperature, g_ctx.humidity);
                sd_write_file(DHT_LOG_FILE_PATH, log_buf, "a");
            }
        }

        // 10. Redondance avec SHT31
        if (sht31_get_runtime() == NULL || !sht31_get_runtime()->valid)
        {
            thermostat_update_indoor_data(g_ctx.temperature, g_ctx.humidity, true);
        }

        // --- GESTION DE L'INTERVALLE ET SYNCHRONISATION ---
        uint32_t configured_delay = g_cfg.dht_read_int_ms;

        // Sécurité matérielle (Le protocole DHT exige au moins 2 secondes entre les lectures)
        if (configured_delay < 2000)
        {
            configured_delay = 2000;
        }

        // Synchronisation sécurisée avec la structure de configuration de tâche
        if (task_config && task_config->delay_ms)
        {
            *task_config->delay_ms = configured_delay;
        }

        // Application de la temporisation
        vTaskDelay(pdMS_TO_TICKS(configured_delay));
    }
#endif
}
