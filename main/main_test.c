#include "unity.h"
#include "esp_log.h"

void app_main(void) {
    printf("\n=== Lancement des tests unitaires ===\n");
    
    // Unity scanne tous les TEST_CASE enregistrés dans le binaire
    unity_run_tests_by_tag("[serial]", false);
    
    printf("\n=== Tests termines ===\n");
}

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
static void dump_nvs_info(void)
{
    nvs_stats_t stats;
    if (nvs_get_stats(NULL, &stats) != ESP_OK)
    {
        ESP_LOGW(TAG, "Impossible de récupérer les stats NVS");
        return;
    }

    ESP_LOGI(TAG, "NVS - Entrées utilisées: %d, Libres: %d, Total: %d",
             stats.used_entries, stats.free_entries, stats.total_entries);

    nvs_iterator_t it = NULL;
    nvs_entry_info_t info;
    if (nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it) != ESP_OK)
    {
        ESP_LOGI(TAG, "Aucune entrée NVS trouvée.");
        return;
    }

    ESP_LOGI(TAG, "---- Détails des entrées NVS ----");
    while (it != NULL)
    {
        nvs_entry_info(it, &info);
        ESP_LOGI(TAG, "Namespace: %-10s | Key: %-20s | Type: %d",
                 info.namespace_name, info.key, info.type);
        nvs_entry_next(&it);
    }
    ESP_LOGI(TAG, "---------------------------------");
}

// --- Tâche pour afficher l'heure (optionnelle) ---
[[maybe_unused]] static void time_log_task(void *pvParameters)
{
    char heure[20] = {0};
    while (1)
    {
        time_utils_get_time_str(heure, sizeof(heure));
        ESP_LOGI(TAG, "Heure actuelle: %s", heure);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void check_system(void)
{
    board_health_t health = alert_get_board_health();
    ESP_LOGI("SYS", "État du système : %s", alert_health_to_str(health));

    if (health >= HEALTH_CRITICAL)
    {
        // Logique de sécurité : par exemple, couper le relais du chauffage
        // heating_emergency_stop();
    }
}

/**
 * @brief Teste l'upload d'un fichier vers la Freebox.
 */
void test_freebox(void) {
    const char *filename = "test_esp.txt";
    const char *content = "Hello Freebox ! Ceci est un fichier envoyé depuis mon ESP32.\n";
    size_t content_len = strlen(content);

    ESP_LOGI(TAG, "Tentative d'upload du fichier '%s' (%u octets)...", filename, content_len);

    esp_err_t err = freebox_ftp_upload(filename, content, content_len);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Succès ! Le fichier '%s' a été uploadé.", filename);
    } else {
        ESP_LOGE(TAG, "Échec upload du fichier '%s': %s", filename, esp_err_to_name(err));
    }
}

/**
 * @brief Télécharge et affiche le contenu d'un fichier depuis la Freebox.
 * @note Alloue dynamiquement un buffer pour stocker les données.
 */
void lecture_freebox(void) {
    const char *FILENAME_TO_DOWNLOAD = "test_esp32.txt";
    const size_t BUFFER_SIZE = 2048;  // Doit correspondre à BUF_SIZE dans freebox_ftp.h

    // Allocation du buffer
    char *buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DEFAULT);
    if (!buffer) {
        ESP_LOGE(TAG, "Échec allocation mémoire pour le buffer");
        return;
    }

    size_t bytes_read = 0;
    esp_err_t err = freebox_ftp_download(FILENAME_TO_DOWNLOAD, buffer, BUFFER_SIZE, &bytes_read);

    // Gestion des erreurs
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec téléchargement du fichier '%s': %s",
                 FILENAME_TO_DOWNLOAD, esp_err_to_name(err));
        free(buffer);
        return;
    }

    // Vérifie que le fichier n'est pas vide ou trop grand
    if (bytes_read == 0) {
        ESP_LOGW(TAG, "Fichier '%s' vide ou introuvable", FILENAME_TO_DOWNLOAD);
        free(buffer);
        return;
    }

    if (bytes_read >= BUFFER_SIZE) {
        ESP_LOGE(TAG, "Fichier trop grand (%u octets >= %u)", bytes_read, BUFFER_SIZE);
        free(buffer);
        return;
    }

    // Terminaison de chaîne (si le fichier est du texte)
    buffer[bytes_read] = '\0';

    // Affichage du contenu
    ESP_LOGI(TAG, "Contenu du fichier '%s' (%u octets):\n%s",
             FILENAME_TO_DOWNLOAD, (unsigned)bytes_read, buffer);

    // Libération de la mémoire
    free(buffer);
}

/**
 * Affiche la liste des fichiers présents sur le serveur
 */
void app_ftp_list_files(void) {
    ESP_LOGI(TAG, "--- Liste des fichiers ---");
    
    // Allocation sur le tas pour éviter le stack overflow
    char *list_buffer = malloc(4096); 
    if (list_buffer == NULL) {
        ESP_LOGE(TAG, "Mémoire insuffisante pour lister les fichiers");
        return;
    }

    if (freebox_ftp_list(list_buffer, 4096) == ESP_OK) {
        ESP_LOGI(TAG, "Fichiers sur la Freebox:\n%s", list_buffer);
    } else {
        ESP_LOGE(TAG, "Échec de la récupération de la liste");
    }

    free(list_buffer);
}

/**
 * Supprime un fichier spécifique
 */
void app_ftp_delete_file(const char *filename) {
    ESP_LOGI(TAG, "Tentative de suppression de : %s", filename);
    
    if (freebox_ftp_delete(filename) == ESP_OK) {
        ESP_LOGI(TAG, "Suppression réussie !");
    } else {
        ESP_LOGE(TAG, "Impossible de supprimer le fichier");
    }
}

/**
 * Renomme un fichier
 */
void app_ftp_rename_file(const char *old_name, const char *new_name) {
    ESP_LOGI(TAG, "Renommage de %s en %s...", old_name, new_name);
    
    if (freebox_ftp_rename(old_name, new_name) == ESP_OK) {
        ESP_LOGI(TAG, "Fichier renommé avec succès !");
    } else {
        ESP_LOGE(TAG, "Échec du renommage");
    }
}


