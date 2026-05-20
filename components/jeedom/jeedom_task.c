#include "jeedom_task.h"

#include "esp_log.h"
#include "freertos/task.h"
#include "jeedom.h"

static const char *TAG = "JEEDOM_TASK";

void jeedom_send_task(void *pvParameters)
{
    jeedom_task_config_t *config = (jeedom_task_config_t *)pvParameters;

    while (1)
    {
        xEventGroupWaitBits(config->event_group, config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        if (config->is_wifi_connected && !config->is_wifi_connected())
        {
            ESP_LOGW(TAG, "Jeedom: WiFi non connecte, attente...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Envoi des donnees a Jeedom...");
        SendStatusJeedom();

        vTaskDelay(pdMS_TO_TICKS(*config->delay_ms));
    }
}
