#include "nvs_flash.h"
#include "esp_log.h"
#include "time_utils_storage.h"

static const char *TAG = "MAIN_TEST";

void test_storage(void)
{
    // 1. Initialisation obligatoire du stockage NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    time_utils_config_t my_cfg;

    // 2. Tentative de chargement
    ESP_LOGI(TAG, "Tentative de chargement de la config...");
    if (time_utils_storage_load(&my_cfg)) {
        ESP_LOGI(TAG, "Configuration existante trouvée !");
    } else {
        ESP_LOGW(TAG, "Aucune config trouvée, initialisation des valeurs par défaut...");
        
        // Initialisation manuelle pour le premier enregistrement
        strlcpy(my_cfg.ntp_server, "pool.ntp.org", sizeof(my_cfg.ntp_server));
        my_cfg.ntp_max_retry = 10;
        my_cfg.ntp_sync_interval_sec = 3600;

        // 3. Sauvegarde des valeurs par défaut
        if (time_utils_storage_save(&my_cfg)) {
            ESP_LOGI(TAG, "Valeurs par défaut sauvegardées avec succès.");
        } else {
            ESP_LOGE(TAG, "Échec de la sauvegarde.");
        }
    }

    // 4. Affichage final pour vérification
    ESP_LOGI(TAG, "--- Paramètres Actuels ---");
    ESP_LOGI(TAG, "Serveur: %s", my_cfg.ntp_server);
    ESP_LOGI(TAG, "Retries: %d", my_cfg.ntp_max_retry);
    ESP_LOGI(TAG, "Intervalle: %d s", (int)my_cfg.ntp_sync_interval_sec);
}