#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freebox_ftp.h"
#include "freebox_report.h"
#include "tasks.h"
#include "time_utils.h" // Ajout de l'include pour le timestamp et la gestion du temps
#include <thermostat.h>

static const char *TAG_SYNC = "FREEBOX_SYNC";

/* =========================================================================
 * 1. VARIABLES STATIQUES DE PASSAGE
 * =========================================================================
 * Ces variables mémorisent la mesure instantanée pour le callback FTP.
 */
static uint64_t s_current_ts = 0;
static float s_current_temp = 0.0f;
static float s_current_hum = 0.0f;
static float s_current_temp_ext = 0.0f;
static float s_current_hum_ext = 0.0f;
static float s_current_forecast_1h = 0.0f;
static float s_current_consigne = 0.0f;
static int s_current_state = 0;

/**
 * @brief Génère le nom du fichier FTP basé sur la date actuelle (ex: "historique_30_05_2026.csv")
 * @param buffer Buffer de destination pour le nom du fichier
 * @param max_len Taille maximale du buffer de destination
 */
static void get_daily_filename(char *buffer, size_t max_len)
{
    char time_str[24];
    // Récupère la chaîne de caractères au format "JJ/MM/AAAA HH:MM:SS"
    time_utils_get_time_str(time_str, sizeof(time_str));
    
    // Transformation de "JJ/MM/AAAA..." en "JJ_MM_AAAA" pour un nom de fichier valide
    time_str[2] = '_';
    time_str[5] = '_';
    time_str[10] = '\0'; // Coupe la chaîne juste après l'année pour ignorer l'heure
    
    snprintf(buffer, max_len, "historique_%s.csv", time_str);
}

/**
 * @brief Callback d'édition FTP : ajoute une ligne de données thermiques complètes pour le modèle 2R2C
 */
static void append_current_measure_callback(char *buffer, size_t *len)
{
    if (!buffer || !len) return;

    size_t current_len = *len;
    size_t max_buffer_size = 65536; // Buffer de 64 KB sécurisé pour 1 pt/min

    // 1. Initialisation de l'en-tête complet si le fichier est neuf
    if (current_len <= 50) {
        current_len = snprintf(buffer, max_buffer_size, 
                               "Timestamp;Temp_Int;Hum_Int;Temp_Ext;Hum_Ext;Forecast_1h;Consigne;Relais_State\n");
        ESP_LOGI("FTP_CALLBACK", "Nouveau fichier quotidien enrichi 2R2C créé.");
    }

    // 2. Écriture de la ligne CSV
    int written = snprintf(buffer + current_len, max_buffer_size - current_len, 
                           "%llu;%.2f;%.1f;%.2f;%.1f;%.2f;%.2f;%d\n",
                           s_current_ts, 
                           s_current_temp, 
                           s_current_hum, 
                           s_current_temp_ext, 
                           s_current_hum_ext, 
                           s_current_forecast_1h, 
                           s_current_consigne, 
                           s_current_state);
    
    if (written > 0 && (current_len + written) < max_buffer_size) {
        current_len += written;
    } else {
        ESP_LOGE("FTP_CALLBACK", "Erreur : Buffer de travail FTP saturé (limite 64KB).");
    }

    *len = current_len; // Met à jour la taille pour l'envoi
}

/**
 * @brief Tâche FreeRTOS de synchronisation périodique de l'historique vers la Freebox
 * @param pvParameters Pointeur vers l'instance globale de thermostat_runtime_t
 */
void freebox_history_sync_task(void *pvParameters)
{
    thermostat_runtime_t *res = (thermostat_runtime_t *)pvParameters;
    if (!res) {
        ESP_LOGE(TAG_SYNC, "Structure thermostat_runtime_t NULL, arrêt de la tâche");
        vTaskDelete(NULL);
        return;
    }

    // Recherche de la configuration de cette tâche pour lier le délai dynamique
    task_info_t *t_info = NULL;
    for (int i = 0; i < TASK_COUNT; i++) {
        if (strcmp(my_tasks[i].key, "ftp_sync") == 0) {
            t_info = &my_tasks[i];
            break;
        }
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();
    char filename[32];

    ESP_LOGI(TAG_SYNC, "Tâche de synchronisation 2R2C initialisée");

    while (1)
    {
        // Attente bloquante par le bit d'activation général
        EventGroupHandle_t task_events = tasks_get_event_group();
        if (task_events != NULL) {
            xEventGroupWaitBits(task_events, BIT_FTP_SYNC_EN, pdFALSE, pdTRUE, portMAX_DELAY);
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // RAZ du compteur de temps suite au déblocage du bit
        xLastWakeTime = xTaskGetTickCount();

        // Récupération de la période (ex: 60000 ms pour 1 minute)
        uint32_t current_delay = (t_info) ? t_info->delay_ms : 60000;
        
        // Attente stricte du cycle d'une minute
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(current_delay));
        
        // 1. Capture instantanée de l'intégralité des variables physiques du thermostat
        s_current_ts          = time_utils_get_timestamp();
        s_current_temp        = res->temperature;
        s_current_hum         = res->humidity;
        s_current_temp_ext    = res->temp_ext;
        s_current_hum_ext     = res->humidity_ext;
        s_current_forecast_1h = res->temp_forecast_1h;
        s_current_consigne    = res->effective_consigne;
        s_current_state       = res->state ? 1 : 0; // Convertit le booléen en 0 ou 1 pour le CSV

        // 2. Génération dynamique du nom de fichier quotidien (ex: historique_30_05_2026.csv)
        get_daily_filename(filename, sizeof(filename));

        // 3. Exécution de la transaction Read-Modify-Write sur la Freebox
        esp_err_t err = freebox_ftp_edit(filename, append_current_measure_callback);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG_SYNC, "✅ Données 2R2C écrites dans %s", filename);
        } else {
            ESP_LOGE(TAG_SYNC, "❌ Erreur FTP pour le fichier '%s' (Code: %d)", filename, err);
        }
    }

    vTaskDelete(NULL);
}
