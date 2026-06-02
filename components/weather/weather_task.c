// #include "weather_task.h"

// #include <time.h>

// #include "esp_log.h"
// #include "freertos/task.h"

// static const char *TAG = "WEATHER_TASK";

// // void weather_update_task(void *pvParameters)
// // {
// //     weather_task_config_t *config = (weather_task_config_t *)pvParameters;

// //     while (1)
// //     {
// //         time_t now;
// //         time(&now);
// //         if (now < 1609459200)
// //         {
// //             ESP_LOGW(TAG, "Heure non synchronisee. Attente...");
// //             vTaskDelay(5000 / portTICK_PERIOD_MS);
// //             continue;
// //         }

// //         xEventGroupWaitBits(config->event_group, config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

// //         if (config->is_wifi_connected && !config->is_wifi_connected())
// //         {
// //             ESP_LOGW(TAG, "Weather: WiFi non connecte, attente...");
// //             vTaskDelay(pdMS_TO_TICKS(2000));
// //             continue;
// //         }

// //         ESP_LOGI(TAG, "Demarrage du cycle de mise a jour meteo...");

// //         weather_data_t tmp_data;
// //         esp_err_t ret = weather_update(&tmp_data);

// //         if (ret == ESP_OK)
// //         {
// //             if (config->store_set_all)
// //                 config->store_set_all(&tmp_data);

// //             ESP_LOGI(TAG, "Meteo mise a jour avec succes.");
// //         }
// //         else
// //         {
// //             ESP_LOGE(TAG, "Echec de la mise a jour meteo (Erreur: %s)", esp_err_to_name(ret));
// //         }

// //         vTaskDelay(pdMS_TO_TICKS(*config->delay_ms));
// //     }
// // }

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

//         ESP_LOGI(TAG, "Demarrage du cycle de mise a jour meteo globale...");

//         // CORRECTION CRUCIALE : Allocation sur le Tas (Heap) pour soulager la pile de la tâche
//         weather_data_t *tmp_data = malloc(sizeof(weather_data_t));
//         if (tmp_data == NULL)
//         {
//             ESP_LOGE(TAG, "Erreur d'allocation memoire pour le cycle meteo !");
//             vTaskDelay(pdMS_TO_TICKS(5000));
//             continue;
//         }
        
//         // Initialisation de la mémoire allouée
//         memset(tmp_data, 0, sizeof(weather_data_t));

//         // 1. Première étape : Mise à jour Open-Meteo
//         // (On passe directement le pointeur tmp_data maintenant)
//         esp_err_t ret = weather_update(tmp_data);

//         if (ret == ESP_OK)
//         {
//             ESP_LOGI(TAG, "Meteo Open-Meteo mise a jour. Enchainement avec Jeedom...");

//             // 2. Deuxième étape : On passe le MEME pointeur à Jeedom
//             esp_err_t ret_jee = jeedom_temp_update(tmp_data);
            
//             if (ret_jee != ESP_OK)
//             {
//                 ESP_LOGW(TAG, "Echec de la recuperation Jeedom, mais on garde Open-Meteo.");
//             }

//             // 3. Étape finale : On pousse la structure complète dans le store
//             if (config->store_set_all)
//                 config->store_set_all(tmp_data);

//             ESP_LOGI(TAG, "Cycle complet de mise a jour reussi.");
//         }
//         else
//         {
//             ESP_LOGE(TAG, "Echec de la mise a jour Open-Meteo (Erreur: %s).", esp_err_to_name(ret));
//         }

//         // CORRECTION CRUCIALE : Libération obligatoire de la mémoire libérée
//         free(tmp_data);
//         tmp_data = NULL;

//         vTaskDelay(pdMS_TO_TICKS(*config->delay_ms));
//     }
// }

#include "weather_task.h"
#include <time.h>
#include "esp_log.h"
#include "freertos/task.h"
#include "esp_heap_caps.h" // AJOUTÉ : Pour la gestion résiliente de la mémoire

static const char *TAG = "WEATHER_TASK";

void weather_update_task(void *pvParameters)
{
    weather_task_config_t *config = (weather_task_config_t *)pvParameters;

    // Sécurité : Si la configuration est nulle, on détruit proprement la tâche
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Configuration de la tache manquante ! Abandon.");
        vTaskDelete(NULL);
    }

    while (1)
    {
        time_t now;
        time(&now);
        if (now < 1609459200)
        {
            ESP_LOGW(TAG, "Heure non synchronisee. Attente...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // Attente de l'événement de déclenchement
        xEventGroupWaitBits(config->event_group, config->event_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        // Vérification de l'état de la connexion WiFi
        if (config->is_wifi_connected && !config->is_wifi_connected())
        {
            ESP_LOGW(TAG, "Weather: WiFi non connecte, attente...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "Demarrage du cycle de mise a jour meteo globale...");

        // CORRECTION 1 : Allocation résiliente (Priorité PSRAM/SPIRAM, sinon RAM interne)
        weather_data_t *tmp_data = heap_caps_malloc(sizeof(weather_data_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (tmp_data == NULL)
        {
            tmp_data = malloc(sizeof(weather_data_t));
        }

        if (tmp_data == NULL)
        {
            ESP_LOGE(TAG, "Erreur critique : Allocation memoire impossible pour le cycle meteo !");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        
        // Initialisation de la structure propre
        memset(tmp_data, 0, sizeof(weather_data_t));

        // 1. Première étape : Mise à jour Open-Meteo
        esp_err_t ret = weather_update(tmp_data);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Meteo Open-Meteo mise a jour. Enchainement avec Jeedom...");

            // 2. Deuxième étape : Récupération des données Jeedom
            esp_err_t ret_jee = jeedom_temp_update(tmp_data);
            
            if (ret_jee != ESP_OK)
            {
                ESP_LOGW(TAG, "Echec de la recuperation Jeedom, mais on garde Open-Meteo.");
            }

            // 3. Étape finale : Enregistrement dans le stockage global
            if (config->store_set_all)
            {
                config->store_set_all(tmp_data);
            }

            ESP_LOGI(TAG, "Cycle complet de mise a jour reussi.");
        }
        else
        {
            ESP_LOGE(TAG, "Echec de la mise a jour Open-Meteo (Erreur: %s).", esp_err_to_name(ret));
        }

        // CORRECTION 2 : Nettoyage centralisé impératif de la mémoire en fin de cycle
        free(tmp_data);
        tmp_data = NULL;

        // CORRECTION 3 : Sécurisation du pointeur de délai et parade anti-boucle folle (si delay_ms vaut NULL ou 0)
        uint32_t delay_duration_ms = 60000; // Valeur par défaut de secours (1 minute)
        if (config->delay_ms && *config->delay_ms > 0)
        {
            delay_duration_ms = *config->delay_ms;
        }
        else
        {
            ESP_LOGW(TAG, "Delai de rafraichissement invalide ou absent. Utilisation de la valeur par defaut.");
        }

        vTaskDelay(pdMS_TO_TICKS(delay_duration_ms));
    }
}
