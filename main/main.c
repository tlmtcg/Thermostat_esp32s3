#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_server.h"   //  httpd_handle_t
#include "web_server.h"        //  start_webserver()
#include "wifi_app.h"          //  ton module WiFi

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
    ESP_LOGI(TAG, "Initialisation du système...");

    // --- Initialisation NVS ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    dump_nvs_info();

    // --- Démarrage du WiFi ---
    ESP_LOGI(TAG, "Démarrage du module WiFi...");
    wifi_app_start();

    // --- Démarrage du serveur Web ---
    ESP_LOGI(TAG, "Démarrage du serveur Web...");
    httpd_handle_t server = start_webserver();

    if (server == NULL)
    {
        ESP_LOGE(TAG, "Le serveur Web n'a pas pu démarrer !");
    }
    else
    {
        ESP_LOGI(TAG, "Serveur Web opérationnel.");
    }

    // --- Boucle principale (optionnelle) ---
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
