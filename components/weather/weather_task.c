#include "weather_task.h"

#include <time.h>

#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "WEATHER_TASK";

void weather_update_task(void *pvParameters)
{
    weather_task_config_t *config = (weather_task_config_t *)pvParameters;

    while (1)
    {
        time_t now;
        time(&now);
        if (now < 1609459200)
        {
            ESP_LOGW(TAG, "Heure non synchronisee. Attente...");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        xEventGroupWaitBits(config->event_group, config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        if (config->is_wifi_connected && !config->is_wifi_connected())
        {
            ESP_LOGW(TAG, "Weather: WiFi non connecte, attente...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Demarrage du cycle de mise a jour meteo...");

        weather_data_t tmp_data;
        esp_err_t ret = weather_update(&tmp_data);

        if (ret == ESP_OK)
        {
            if (config->store_set_all)
                config->store_set_all(&tmp_data);

            ESP_LOGI(TAG, "Meteo mise a jour avec succes.");
        }
        else
        {
            ESP_LOGE(TAG, "Echec de la mise a jour meteo (Erreur: %s)", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(*config->delay_ms));
    }
}
