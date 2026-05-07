#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "weather.h"
#include "weather_store.h"
#include "weather_utils.h"
#include "jeedom.h"
#include "task_manager.h"
#include "time_utils.h"
#include <time.h>
#include "esp_sntp.h"

static const char *TAG = "TASK_MGR";

/* -------------------------------------------------------------------------- */
/*                         TÂCHE : Mise à jour météo                          */
/* -------------------------------------------------------------------------- */

static void weather_update_task(void *pvParameters)
{
    (void)pvParameters;

    weather_data_t tmp;

    while (1)
    {
        ESP_LOGI(TAG, "Mise à jour météo via TaskManager...");

        /* --- 1. Récupération Open-Meteo --- */
        if (weather_update(&tmp) == ESP_OK)
        {
            weather_store_set_all(&tmp);

            log_weather_entry("MÉTÉO ACTUELLE", &tmp.current);
            log_weather_entry("PRÉVISION +48H", &tmp.forecast_48h);

            for (int i = 0; i < 7; i++)
            {
                char label[32];
                snprintf(label, sizeof(label), "PRÉVISION J+%d", i + 1);
                log_weather_entry(label, &tmp.forecast_7j[i]);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Échec de la mise à jour météo.");
        }

        /* --- 2. Récupération température extérieure Jeedom --- */
        ESP_LOGI(TAG, "Mise à jour température extérieure via TaskManager...");

        weather_store_get_all(&tmp);

        if (jeedom_temp_update(&tmp) == ESP_OK)
        {
            weather_store_set_all(&tmp);
            log_weather_entry("TEMPÉRATURE EXTÉRIEURE", &tmp.current);
        }
        else
        {
            ESP_LOGE(TAG, "Échec de la mise à jour température Jeedom.");
        }

        /* --- 3. Pause 15 minutes --- */
        vTaskDelay(pdMS_TO_TICKS(15 * 60 * 1000));
    }
}

/* -------------------------------------------------------------------------- */
/*                   TÂCHE : Envoi périodique vers Jeedom                     */
/* -------------------------------------------------------------------------- */

static void jeedom_send_task(void *pvParameters)
{
    (void)pvParameters;

    /* Laisse le WiFi se stabiliser */
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1)
    {
        ESP_LOGI(TAG, "Envoi du statut à Jeedom...");

        if (SendStatusJeedom())
        {
            ESP_LOGI(TAG, "Statut Jeedom envoyé !");
        }
        else
        {
            ESP_LOGE(TAG, "Erreur lors de l'envoi (Serveur injoignable ou erreur 4xx/5xx)");
        }

        vTaskDelay(pdMS_TO_TICKS(60000)); // 1 minute
    }
}

/* -------------------------------------------------------------------------- */
/*                   TÂCHE : Mise à jour ntp                                  */
/* -------------------------------------------------------------------------- */

static void ntp_monitor_task(void *pvParameters)
{
    while (1)
    {
        time_t now;
        time(&now);

        time_t last = time_utils_get_last_sync();

        if (last == 0 || (now - last) > 3600)
        {
            ESP_LOGI(TAG, "SNTP init (auto recovery)");
            esp_sntp_init();  // suffit
        }

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

/* -------------------------------------------------------------------------- */
/*                           INITIALISATION DU MODULE                         */
/* -------------------------------------------------------------------------- */

void task_manager_init(void)
{
    ESP_LOGI(TAG, "Initialisation des tâches système...");

    /* Initialise le store météo (mutex + structure interne) */
    weather_store_init();

    /* Tâche météo (toutes les 15 minutes) */
    xTaskCreate(weather_update_task, "weather_task", 8192, NULL, 5, NULL);

    /* Tâche Jeedom (toutes les 1 minute) */
    xTaskCreate(jeedom_send_task, "jeedom_task", 4096, NULL, 5, NULL);

    /* Tâche Mise à jour ntp (toute les 1 minutes) */
    xTaskCreate(ntp_monitor_task, "ntp_task", 4096, NULL, 5, NULL);
}
