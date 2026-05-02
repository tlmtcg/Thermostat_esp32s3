#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "web_server.h"
#include "wifi_app.h"
#include "time_utils.h"
#include "led_ctrl.h"  // Module LED refactorisé
#include "esp_littlefs.h"

static const char *TAG = "MAIN_APP";

// --- Initialisation littlefs ---
// static esp_err_t init_littlefs(void) {
//     ESP_LOGI(TAG, "Initialisation littlefs...");

//     esp_vfs_littlefs_conf_t conf = {
//         .base_path = "/littlefs",
//         .partition_label = NULL,  // Utilise la partition par défaut
//         .format_if_mount_failed = true
//     };

//     esp_err_t ret = esp_vfs_littlefs_register(&conf);
//     if (ret != ESP_OK) {
//         if (ret == ESP_FAIL) {
//             ESP_LOGE(TAG, "Échec du montage littlefs");
//         } else if (ret == ESP_ERR_NOT_FOUND) {
//             ESP_LOGE(TAG, "Partition littlefs introuvable");
//         } else {
//             ESP_LOGE(TAG, "Erreur littlefs: %s", esp_err_to_name(ret));
//         }
//         return ret;
//     }

//     size_t total = 0, used = 0;
//     ret = esp_littlefs_info(NULL, &total, &used);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Impossible de récupérer les infos littlefs (%s)", esp_err_to_name(ret));
//         return ret;
//     }

//     ESP_LOGI(TAG, "littlefs initialisé: Total=%d, Utilisé=%d", total, used);
//     return ESP_OK;
// }

// --- Affichage des infos NVS ---
static void dump_nvs_info(void) {
    nvs_stats_t stats;
    if (nvs_get_stats(NULL, &stats) != ESP_OK) {
        ESP_LOGW(TAG, "Impossible de récupérer les stats NVS");
        return;
    }

    ESP_LOGI(TAG, "NVS - Entrées utilisées: %d, Libres: %d, Total: %d",
             stats.used_entries, stats.free_entries, stats.total_entries);

    nvs_iterator_t it = NULL;
    nvs_entry_info_t info;
    if (nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it) != ESP_OK) {
        ESP_LOGI(TAG, "Aucune entrée NVS trouvée.");
        return;
    }

    ESP_LOGI(TAG, "---- Détails des entrées NVS ----");
    while (it != NULL) {
        nvs_entry_info(it, &info);
        ESP_LOGI(TAG, "Namespace: %-10s | Key: %-20s | Type: %d",
                 info.namespace_name, info.key, info.type);
        nvs_entry_next(&it);
    }
    ESP_LOGI(TAG, "---------------------------------");
}

// --- Tâche pour afficher l'heure (optionnelle) ---
static void time_log_task(void *pvParameters) {
    char heure[20] = {0};
    while (1) {
        time_utils_get_time_str(heure, sizeof(heure));
        ESP_LOGI(TAG, "Heure actuelle: %s", heure);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Démarrage du système...");

    // --- 1. Initialisation NVS (obligatoire pour WiFi, stockage, etc.) ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    dump_nvs_info();

        // --- 2. Initialisation du module LED (doit être fait AVANT le WiFi si dépendances) ---
    ESP_LOGI(TAG, "Initialisation du module LED...");
    ret = led_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec de l'initialisation LED: %s", esp_err_to_name(ret));
        // Gérer l'erreur (ex: redémarrer ou continuer sans LED)
    } else {
        // Allumer la LED en bleu pour indiquer que le système démarre
        led_set_background(LED_MODE_BREATH, (led_color_t){0, 0, 50}, 1000);
    }

    // --- 3. Démarrage du WiFi ---
    ESP_LOGI(TAG, "Démarrage du module WiFi...");
    wifi_app_start();

    // --- 4. Démarrage du serveur Web ---
    ESP_LOGI(TAG, "Démarrage du serveur Web...");
    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Échec du démarrage du serveur Web !");
        led_set_background(LED_MODE_FIXED, (led_color_t){255, 0, 0}, 1000);  // LED rouge = erreur
    } else {
        ESP_LOGI(TAG, "Serveur Web opérationnel.");
        led_set_background(LED_MODE_FIXED, (led_color_t){0, 50, 0}, 1000);  // LED verte = OK
    }

    // --- 5. Initialisation du SNTP (après WiFi) ---
    ESP_LOGI(TAG, "Démarrage du SNTP...");
    time_utils_init();

    // --- 6. Démarrage de la tâche d'affichage de l'heure (optionnelle) ---
    xTaskCreate(time_log_task, "TimeLogTask", 2048, NULL, 1, NULL);

    // --- 7. Afficher l'état de la base de données
    led_db_print_status();

    // --- 8. Boucle principale (optionnelle) ---
    // Dans ce cas, tout est géré par des tâches FreeRTOS, donc on peut supprimer la boucle while(1)
    // Si vous voulez garder une boucle, utilisez un délai pour éviter de bloquer le CPU
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Délai de 10 secondes
        ESP_LOGI(TAG, "Système opérationnel...");
    }
}
