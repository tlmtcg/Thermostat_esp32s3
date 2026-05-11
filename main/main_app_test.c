#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

// Tes composants
#include "wifi_manager.h" 
#include "wifi_app.h"
#include "email_service.h"
#include "sd_card.h"

static const char *TAG = "TEST_APP";

// Prototype de la fonction de test
void test_email(void);

// 1. RENOMMÉ EN app_main (Obligatoire pour le boot)
void app_main(void) {
    // Initialisation NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialisation Réseau
    ESP_ERROR_CHECK(esp_netif_init());

    // Démarrage WiFi
    ESP_LOGI(TAG, "Connexion au WiFi...");
    wifi_app_start();

    // Attente de l'obtention de l'IP (ajuste selon ton besoin)
    ESP_LOGI(TAG, "Attente de l'IP (5s)...");
    vTaskDelay(pdMS_TO_TICKS(5000)); 

       if (init_sd_card(NULL) != ESP_OK)
    {
        ESP_LOGW(TAG, "La carte SD n'est pas disponible. Les fonctionnalités de stockage seront limitées.");
        return;
    }

    // Lancer le test
    test_email();
}

// Exemple de callback pour l'email
void on_email_done(bool success) {
    if (success) ESP_LOGI(TAG, "Email envoyé !");
    else ESP_LOGE(TAG, "Échec email !");
}

void test_email(void)
{
    ESP_LOGI(TAG, "Initialisation email_service...");
    email_service_init();

    const char *target_file = "/sdcard/alerts.log"; 

    ESP_LOGI(TAG, "--- Vérification du contenu de la carte SD ---");
    sd_list_files("/sdcard");

    ESP_LOGI(TAG, "Envoi de l'email...");
    email_send_log_async(
        "dup.cordon@gmail.com",
        "ESP32 Logs",
        "Ceci est le corps du mail",
       target_file       
    );
}