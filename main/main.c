// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/timers.h"
// #include "wifi_manager.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "nvs.h"
// #include "esp_wifi.h"

// static const char *TAG = "MAIN_APP";

// // --- GLOBAL ---
// TimerHandle_t xReconnectTimer = NULL;

// // --- CALLBACKS & HELPERS ---

// void dump_nvs_info()
// {
//     nvs_stats_t nvs_stats;
//     nvs_get_stats(NULL, &nvs_stats);
//     printf("NVS - Entrées utilisées : %d, Libres : %d, Total : %d\n",
//            nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
// }

// void vTimerReconnectCallback(TimerHandle_t xTimer)
// {
//     int count = wifi_get_ap_client_count();

//     if (count > 0)
//     {
//         ESP_LOGW("MAIN", "Utilisateur présent sur l'AP (%d). On ne perturbe pas la radio.", count);
//         xTimerStart(xReconnectTimer, 0); // On remet 5s
//     }
//     else
//     {
//         ESP_LOGI("MAIN", "AP libre. Tentative reconnexion...");
//         esp_wifi_connect();
//     }
// }

// void my_on_connected(const esp_ip4_addr_t *ip)
// {
//     ESP_LOGI(TAG, "Connecté ! IP : " IPSTR, IP2STR(ip));
//     if (xReconnectTimer)
//         xTimerStop(xReconnectTimer, 0);

//     // start_webserver();
// }

// void my_on_failed(int reason)
// {
//     ESP_LOGE(TAG, "Liaison perdue (raison: %d). Retry dans 5s...", reason);
//     if (xReconnectTimer)
//         xTimerStart(xReconnectTimer, 0);
// }

// // --- MÉTHODE REGROUPÉE ---

// /**
//  * Initialise le timer de reconnexion et lance le WiFi Manager
//  */
// void app_wifi_start(void)
// {
//     // 1. Création du timer de retry
//     xReconnectTimer = xTimerCreate("WiFi_Retrier",
//                                    pdMS_TO_TICKS(5000),
//                                    pdFALSE,
//                                    (void *)0,
//                                    vTimerReconnectCallback);

//     // 2. Définition des callbacks (static pour persistance)
//     static wifi_callbacks_t cb = {0};
//     cb.on_sta_connected = my_on_connected;
//     cb.on_sta_failed = my_on_failed;
//     cb.on_ap_started = NULL;

//     // 3. Lancement
//     ESP_LOGI(TAG, "Démarrage du sous-système WiFi...");
//     wifi_manager_init(&cb);
// }

// // --- ENTRY POINT ---

// void app_main(void)
// {
//     ESP_LOGI(TAG, "--- Démarrage Thermostat ESP32-S3 ---");

//     // Info mémoire flash
//     dump_nvs_info();

//     // Initialisation WiFi isolée
//     app_wifi_start();

//     // Ici, tu pourras ajouter tes autres services
//     // app_sensors_init();
//     // app_display_init();

//     ESP_LOGI(TAG, "app_main terminé, le système tourne en arrière-plan.");
// }

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "wifi_app.h"

static const char *TAG = "MAIN_APP";

static void dump_nvs_info()
{
    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);
    printf("NVS - Entrées utilisées : %d, Libres : %d, Total : %d\n",
           nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
}

void app_main(void)
{
    ESP_LOGI(TAG, "--- Démarrage Thermostat ESP32-S3 ---");

    dump_nvs_info();

    // Démarrage WiFi
    wifi_app_start();

    ESP_LOGI(TAG, "app_main terminé, tâches en arrière-plan.");
}
