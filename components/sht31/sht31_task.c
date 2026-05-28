#include "sht31_task.h"

#include <stdio.h>

#include "app_context.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sd_card.h"
#include "time_utils.h"

// =========================================================================
// CONFIGURATION : METTRE À 0 SI LE CAPTEUR SHT31 N'EST PAS CÂBLÉ PHYSIQUEMENT
// =========================================================================
#define USE_SHT31_SENSOR 1
#define SHT31_DEBUG

static const char *TAG = "SHT31_TASK";

#if USE_SHT31_SENSOR
// L'include et la configuration du fichier matériel ne sont compilés que si le capteur existe
#include "sht31.h"

#define SHT31_LOG_FILE_PATH MOUNT_POINT "/sht31_data.csv"
#define SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS 3
#define LOG_INTERVAL_MS (5 * 60 * 1000)

int64_t last_log_time = 0;
#endif

void sht31_task(void *pvParameters)
{
#if !USE_SHT31_SENSOR
    // Sécurité FreeRTOS : Si le capteur n'est pas branché, on nettoie les structures de données globales et on quitte
    g_ctx.temperature = 0.0f;
    g_ctx.humidity = 0.0f;
    ESP_LOGW(TAG, "Capteur SHT31 désactivé via flag -> Auto-destruction de la tâche.");
    vTaskDelete(NULL);
#else
    sht31_task_config_t *task_config = (sht31_task_config_t *)pvParameters;
    bool was_active = false;

    while (1)
    {
        xEventGroupWaitBits(task_config->event_group, task_config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        if (!was_active)
        {
            sht31_set_running(true);
            was_active = true;
        }

        float temperature = 0.0f;
        float humidity = 0.0f;
        esp_err_t ret = sht31_read(&temperature, &humidity);

        if (ret == ESP_OK)
        {
            sht31_config_t config;
            char time_str[24];
            char log_buffer[128];

            g_ctx.temperature = temperature;
            g_ctx.humidity = humidity;

#ifdef SHT31_DEBUG
            ESP_LOGI(TAG, "SHT31: %.2f C, %.2f%%", temperature, humidity);
#endif

            int64_t now = time_utils_get_timestamp();

            if ((now - last_log_time) >= (LOG_INTERVAL_MS * 1000))
            {
                last_log_time = now;

                #ifdef SHT31_DEBUG
                ESP_LOGI(TAG, "SHT31: %.2f C, %.2f%%", temperature, humidity);
                #endif

                if (sht31_get_config(&config) == ESP_OK && config.log_to_sd)
                {
                    time_utils_get_time_str(time_str, sizeof(time_str));

                    snprintf(log_buffer, sizeof(log_buffer),
                             "%s,%.2f,%.2f\n",
                             time_str, temperature, humidity);

                    if (sd_write_file(SHT31_LOG_FILE_PATH, log_buffer) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Erreur ecriture log SHT31");
                    }
                }
            }
        }
        else
        {
            const sht31_runtime_t *runtime = sht31_get_runtime();

            if (runtime->consecutive_error_count <= SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS ||
                (runtime->consecutive_error_count % 10) == 0)
            {
                ESP_LOGW(TAG,
                         "Erreur SHT31: %s (consecutives=%lu)",
                         esp_err_to_name(ret),
                         (unsigned long)runtime->consecutive_error_count);
            }

            if (runtime->consecutive_error_count >= SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS &&
                (runtime->consecutive_error_count % SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS) == 0)
            {
                esp_err_t recover_ret = sht31_recover();
                if (recover_ret != ESP_OK)
                    ESP_LOGW(TAG, "Recuperation SHT31 echouee: %s", esp_err_to_name(recover_ret));
            }
        }

        sht31_config_t config;
        uint32_t delay_ms = *task_config->delay_ms;

        if (sht31_get_config(&config) == ESP_OK && config.read_interval_ms > 0)
            delay_ms = config.read_interval_ms;

        if (delay_ms == 0)
            delay_ms = 5000;

        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        if ((xEventGroupGetBits(task_config->event_group) & task_config->event_bit) == 0)
        {
            sht31_set_running(false);
            was_active = false;
        }
    }
#endif
}
