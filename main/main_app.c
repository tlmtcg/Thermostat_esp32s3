#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "web_server.h"
#include "wifi_app.h"
#include "time_utils.h"
#include "led_ctrl.h" 
#include "esp_littlefs.h"
#include "alert_manager.h"
#include "task_manager.h"
#include "serial_manager.h"
#include "freebox_ftp.h"
#include "heating_program.h"
#include "sd_card.h"
#include "alert_storage.h"
#include "led_storage.h"
// Includes pour les composants manquants
#include "config_storage.h"   // Pour load_json_from_sdcard et save_kconfig_to_sdcard
#include "i2c_manager.h"      // Pour i2c_manager_init, i2c_manager_scan, i2c_manager_get_devices_json
#include "email_service.h"    // Pour email_send_log_async
#include "cJSON.h"            // Pour manipuler le JSON

static const char *TAG = "MAIN_APP";

// Définition de CONFIG_FILE si non définie dans un header
#ifndef CONFIG_FILE
#define CONFIG_FILE "/sdcard/config.json"
#endif

void test_email();

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

    // 2) Charger la config JSON si elle existe
    cJSON *config_json = load_json_from_sdcard(CONFIG_FILE);

    if (config_json) {
        ESP_LOGI(TAG, "Configuration chargée depuis %s", CONFIG_FILE);

        // TODO : appliquer la config JSON (I2C, SD, etc.)
        // apply_config_json(config_json);

        cJSON_Delete(config_json);
    } else {
        ESP_LOGW(TAG, "Aucune config JSON → génération depuis Kconfig");

        // 3) Sauvegarder la config Kconfig → JSON
        save_kconfig_to_sdcard(CONFIG_FILE);
    }


    // alert_storage_init(MOUNT_POINT "/alerts.log");
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

    // --- 8. Affichage de l'état de la base de données LED ---
    led_db_print_status();

    // --- 9. Test du programme de chauffage ---
    ESP_LOGI(TAG, "Chargement du programme de chauffage ...");
    heating_init(&config);

    // test_email();

     // Initialiser le bus I2C
    i2c_manager_init();

    // Scanner les périphériques I2C
    i2c_manager_scan();

    // Récupérer le JSON
    cJSON *devices_json = i2c_manager_get_devices_json();
    if (devices_json != NULL) {
        char *json_str = cJSON_Print(devices_json);
        ESP_LOGI("MAIN", "JSON des périphériques I2C:\n%s", json_str);
        free(json_str);  // Libérer la chaîne après utilisation
    }

}

void test_email(){
    const char *target_file = "/sdcard/alerts.log"; 

    ESP_LOGI(TAG, "Envoi de l'email...");
    email_send_log_async(
        "dup.cordon@gmail.com",
        "ESP32 Logs",
        "Ceci est le corps du mail",
       target_file       
    );
}