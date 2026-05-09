#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "web_server.h"
#include "wifi_app.h"
#include "time_utils.h"
#include "led_ctrl.h" // Module LED refactorisé
#include "esp_littlefs.h"
#include "alert_manager.h"
#include "task_manager.h"
#include "serial_manager.h"
#include "freebox_ftp.h"
#include "heating_program.h"
#include "sd_card.h"
#include "alert_storage.h"
#include "led_storage.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    ESP_LOGI(TAG, "Démarrage du système...");

    // --- 6. Initialisation des tâches ---
    task_manager_init();

    // --- 1. Initialisation NVS ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // --- 2. Initialisation carte SD ---
    if (init_sd_card(NULL) != ESP_OK)
    {
        ESP_LOGW(TAG, "La carte SD n'est pas disponible. Les fonctionnalités de stockage seront limitées.");
        return;
    }

    alert_storage_init(MOUNT_POINT "/alerts.log");
    led_storage_init();

    // --- 3bis; Initialisation du backend LED (doit être après la SD) ---
    led_init();

    // --- 3. Démarrage du WiFi ---
    ESP_LOGI(TAG, "Démarrage du WiFi...");
    wifi_app_start();

    // --- 4. Démarrage du serveur Web ---
    ESP_LOGI(TAG, "Démarrage du serveur Web...");
    httpd_handle_t server = start_webserver();
    if (server == NULL)
    {
        ESP_LOGE(TAG, "Échec démarrage serveur Web !");
        led_set_background(LED_MODE_FIXED, (led_color_t){255, 0, 0}, 1000); // Rouge = erreur
    }
    else
    {
        ESP_LOGI(TAG, "Serveur Web opérationnel.");
        // led_set_background(LED_MODE_FIXED, (led_color_t){0, 50, 0}, 1000);  // Vert = OK
    }

    // --- 5. Initialisation du SNTP ---
    ESP_LOGI(TAG, "Démarrage du SNTP...");
    time_utils_init();

    // --- 7. Initialisation du serial manager ---
    serial_manager_init();

    // --- 8. Affichage de l'état de la base de données LED ---
    led_db_print_status();

    // --- 9. Test du programme de chauffage ---
    ESP_LOGI(TAG, "Chargement du programme de chauffage ...");
    heating_init(&config);
}