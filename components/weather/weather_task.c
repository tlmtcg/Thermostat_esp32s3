#include "weather_task.h"

#include <time.h>

#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "WEATHER_TASK";

// void weather_update_task(void *pvParameters)
// {
//     weather_task_config_t *config = (weather_task_config_t *)pvParameters;

//     while (1)
//     {
//         time_t now;
//         time(&now);
//         if (now < 1609459200)
//         {
//             ESP_LOGW(TAG, "Heure non synchronisee. Attente...");
//             vTaskDelay(5000 / portTICK_PERIOD_MS);
//             continue;
//         }

//         xEventGroupWaitBits(config->event_group, config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

//         if (config->is_wifi_connected && !config->is_wifi_connected())
//         {
//             ESP_LOGW(TAG, "Weather: WiFi non connecte, attente...");
//             vTaskDelay(pdMS_TO_TICKS(2000));
//             continue;
//         }

//         ESP_LOGI(TAG, "Demarrage du cycle de mise a jour meteo...");

//         weather_data_t tmp_data;
//         esp_err_t ret = weather_update(&tmp_data);

//         if (ret == ESP_OK)
//         {
//             if (config->store_set_all)
//                 config->store_set_all(&tmp_data);

//             ESP_LOGI(TAG, "Meteo mise a jour avec succes.");
//         }
//         else
//         {
//             ESP_LOGE(TAG, "Echec de la mise a jour meteo (Erreur: %s)", esp_err_to_name(ret));
//         }

//         vTaskDelay(pdMS_TO_TICKS(*config->delay_ms));
//     }
// }

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

        ESP_LOGI(TAG, "Demarrage du cycle de mise a jour meteo globale...");

        // CORRECTION CRUCIALE : Allocation sur le Tas (Heap) pour soulager la pile de la tâche
        weather_data_t *tmp_data = malloc(sizeof(weather_data_t));
        if (tmp_data == NULL)
        {
            ESP_LOGE(TAG, "Erreur d'allocation memoire pour le cycle meteo !");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        // Initialisation de la mémoire allouée
        memset(tmp_data, 0, sizeof(weather_data_t));

        // 1. Première étape : Mise à jour Open-Meteo
        // (On passe directement le pointeur tmp_data maintenant)
        esp_err_t ret = weather_update(tmp_data);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Meteo Open-Meteo mise a jour. Enchainement avec Jeedom...");

            // 2. Deuxième étape : On passe le MEME pointeur à Jeedom
            esp_err_t ret_jee = jeedom_temp_update(tmp_data);
            
            if (ret_jee != ESP_OK)
            {
                ESP_LOGW(TAG, "Echec de la recuperation Jeedom, mais on garde Open-Meteo.");
            }

            // 3. Étape finale : On pousse la structure complète dans le store
            if (config->store_set_all)
                config->store_set_all(tmp_data);

            ESP_LOGI(TAG, "Cycle complet de mise a jour reussi.");
        }
        else
        {
            ESP_LOGE(TAG, "Echec de la mise a jour Open-Meteo (Erreur: %s).", esp_err_to_name(ret));
        }

        // CORRECTION CRUCIALE : Libération obligatoire de la mémoire libérée
        free(tmp_data);
        tmp_data = NULL;

        vTaskDelay(pdMS_TO_TICKS(*config->delay_ms));
    }
}
