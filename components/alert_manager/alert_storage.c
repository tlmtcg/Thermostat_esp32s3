#include "alert_storage.h"
#include "alert_manager.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "time_utils.h"

// =========================================================
// CONFIGURATION : ACTIVER (1) OU DÉSACTIVER (0) LA CARTE SD
// =========================================================
#define USE_SD_CARD_STORAGE    1 

static const char *TAG = "ALERT_STORAGE";

#if USE_SD_CARD_STORAGE
#include "sd_card.h"
#include <sys/stat.h>

static const char *log_path = MOUNT_POINT "/alerts.log";
#define MAX_LOG_SIZE_BYTES (200 * 1024) // 200 KB
#define MAX_LOG_LINES 2000

static TaskHandle_t s_alert_task = NULL;
#endif

/* =========================================================
   QUEUE UNIQUEMENT
   ========================================================= */
typedef struct
{
    char line[256]; // ligne JSON déjà formatée
} alert_msg_t;

static QueueHandle_t s_alert_queue = NULL;

/* =========================================================
   TÂCHE D'ÉCRITURE SD (Compilée uniquement si le flag est à 1)
   ========================================================= */

void alert_storage_task(void *arg)
{
    #if USE_SD_CARD_STORAGE
        ESP_LOGI(TAG, "Tâche alert_storage_task démarrée, log_path=%s", log_path);
    for (;;)
    {
        alert_msg_t msg;

        if (xQueueReceive(s_alert_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Réception message SD : %s", msg.line);
            ESP_LOGI(TAG, "Écriture dans : %s", log_path);

            esp_err_t err = sd_write_file(log_path, msg.line,"a");
            if (err != ESP_OK)
            {
                alert_add("Erreur ECRITURE SD");
                ESP_LOGE(TAG, "Erreur ECRITURE SD");
            }

            alert_storage_purge();
        }
    }
    #else
    // Sécurité : Si la tâche est créée par erreur en mode sans SD, 
    // on la détruit proprement pour éviter le crash.
    ESP_LOGW(TAG, "Tâche 'Storage' appelée mais inactive (sans SD) -> Destruction de la tâche.");
    vTaskDelete(NULL);
    #endif
}


/* =========================================================
   CALLBACK DE RÉCEPTION DES ALERTES
   ========================================================= */
static void on_alert_event(alert_event_t evt, const alert_log_t *log)
{
    if (!s_alert_queue || !log)
        return;

    alert_msg_t msg = {0};
    char time_str[32];

    time_utils_get_time_str(time_str, sizeof(time_str));

    // Formatage du JSON identique pour les deux modes
    snprintf(msg.line, sizeof(msg.line),
             "{ \"timestamp\": \"%s\", \"name\": \"%s\", \"activated\": %d }\n",
             time_str,
             log->name,
             log->activated ? 1 : 0);

#if !USE_SD_CARD_STORAGE
    // Si pas de carte SD, on affiche directement l'alerte formatée dans la console
    ESP_LOGI(TAG, "[RAM LOG] %s", msg.line);
#endif

    // Envoi dans la queue (utile pour la tâche SD ou d'autres notifications)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xPortInIsrContext())
    {
        if (xQueueSendFromISR(s_alert_queue, &msg, &xHigherPriorityTaskWoken) != pdPASS)
            ESP_EARLY_LOGW(TAG, "Queue pleine (ISR), événement perdu");

        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR();
    }
    else
    {
        if (xQueueSend(s_alert_queue, &msg, 0) != pdPASS)
            ESP_LOGW(TAG, "Queue pleine, événement perdu");
    }
}

/* =========================================================
   ROTATION
   ========================================================= */
void alert_storage_rotate(void)
{
#if USE_SD_CARD_STORAGE
    char rotated[128];
    snprintf(rotated, sizeof(rotated), "%s.1", log_path);

    sd_delete_file(rotated);
    sd_rename_file(log_path, rotated);

    ESP_LOGW(TAG, "Rotation effectuée : %s → %s", log_path, rotated);
#endif
}

/* =========================================================
   PURGE
   ========================================================= */
void alert_storage_purge(void)
{
#if USE_SD_CARD_STORAGE
    struct stat st;
    if (stat(log_path, &st) != 0)
        return;

    if (st.st_size < MAX_LOG_SIZE_BYTES)
        return;

    ESP_LOGW(TAG, "Fichier trop gros (%ld bytes) → rotation", st.st_size);
    alert_storage_rotate();
#endif
}

/* =========================================================
   INIT
   ========================================================= */
void alert_storage_init(const char *path)
{
    // Création de la queue (commune aux deux modes)
    s_alert_queue = xQueueCreate(32, sizeof(alert_msg_t));
    if (!s_alert_queue)
    {
        ESP_LOGE(TAG, "Impossible de créer la queue");
        return;
    }

    // Enregistrement du callback unique
    alert_register_callback(on_alert_event);

#if USE_SD_CARD_STORAGE
    if (path)
        log_path = path;

    ESP_LOGI(TAG, "Initialisation du stockage AVEC Carte SD : %s", log_path);
    
    // On ne crée la tâche FreeRTOS que si la carte SD est activée
    // xTaskCreate(alert_storage_task, "alert_storage_task", 4096, NULL, 5, &s_alert_task);
#else
    ESP_LOGI(TAG, "Initialisation du stockage SANS Carte SD (Log console uniquement)");
#endif
}

/* =========================================================
   LOAD
   ========================================================= */
void alert_storage_load(void)
{
#if USE_SD_CARD_STORAGE
    FILE *f = fopen(log_path, "r");
    if (!f)
        return;

    alert_clear_all();
    char line[256];
    int line_count = 0;

    while (fgets(line, sizeof(line), f))
    {
        char time_str[32] = {0};
        char name[64] = {0};
        int activated = 0;

        int found = sscanf(line,
                           "{ \"timestamp\": \"%31[^\"]\", \"name\": \"%63[^\"]\", \"activated\": %d }",
                           time_str, name, &activated);

        if (found < 3)
            continue;

        alert_log_t log;
        log.timestamp = 0; 
        strncpy(log.name, name, sizeof(log.name));
        log.activated = (bool)activated;

        alert_push_history(&log);

        if (++line_count > MAX_LOG_LINES)
        {
            fclose(f);
            alert_storage_rotate();
            return;
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "Historique SD chargé (%d lignes)", line_count);
#else
    ESP_LOGD(TAG, "Pas de carte SD active, aucun historique à charger");
#endif
}
