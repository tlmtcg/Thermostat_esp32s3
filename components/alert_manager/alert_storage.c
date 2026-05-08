#include "alert_storage.h"
#include "alert_manager.h"
#include "sd_card.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "ALERT_STORAGE";
static const char *log_path = MOUNT_POINT "/alerts.log";

#define MAX_LOG_SIZE_BYTES (200 * 1024) // 200 KB
#define MAX_LOG_LINES 2000

/* =========================================================
   QUEUE + TASK
   ========================================================= */

typedef struct
{
    char line[256]; // ligne JSON déjà formatée
} alert_sd_msg_t;

static QueueHandle_t s_alert_queue = NULL;
static TaskHandle_t s_alert_task = NULL;

/* =========================================================
   TÂCHE D'ÉCRITURE SD
   ========================================================= */
static void alert_storage_task(void *arg)
{
    alert_sd_msg_t msg;

    ESP_LOGI(TAG, "Tâche alert_storage_task démarrée, log_path=%s", log_path);

    for (;;)
    {
        if (xQueueReceive(s_alert_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Réception message SD : %s", msg.line);
            ESP_LOGI(TAG, "Écriture dans : %s", log_path);

            esp_err_t err = sd_write_file(log_path, msg.line);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Erreur écriture SD");
            }

            alert_storage_purge();
        }
    }
}

/* =========================================================
   CALLBACK ALERTES → PUSH DANS QUEUE
   ========================================================= */
static void sd_on_alert_event(alert_event_t evt, const alert_log_t *log)
{
    if (!s_alert_queue || !log)
        return;

    alert_sd_msg_t msg = {0};

    // Format JSON identique à ton code original
    snprintf(msg.line, sizeof(msg.line),
             "{ \"timestamp\": %lld, \"name\": \"%s\", \"activated\": %d }\n",
             (long long)log->timestamp,
             log->name,
             log->activated ? 1 : 0);

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
    char rotated[128];
    snprintf(rotated, sizeof(rotated), "%s.1", log_path);

    sd_delete_file(rotated);
    sd_rename_file(log_path, rotated);

    ESP_LOGW(TAG, "Rotation effectuée : %s → %s", log_path, rotated);
}

/* =========================================================
   PURGE
   ========================================================= */
void alert_storage_purge(void)
{
    struct stat st;
    if (stat(log_path, &st) != 0)
        return;

    if (st.st_size < MAX_LOG_SIZE_BYTES)
        return;

    ESP_LOGW(TAG, "Fichier trop gros (%ld bytes) → rotation", st.st_size);
    alert_storage_rotate();
}

/* =========================================================
   INIT
   ========================================================= */
void alert_storage_init(const char *path)
{
    if (path)
        log_path = path;

    ESP_LOGI(TAG, "Initialisation du stockage SD : %s", log_path);

    // Création de la queue
    s_alert_queue = xQueueCreate(16, sizeof(alert_sd_msg_t));
    if (!s_alert_queue)
    {
        ESP_LOGE(TAG, "Impossible de créer la queue SD");
        return;
    }

    // Création de la tâche
    BaseType_t ok = xTaskCreate(
        alert_storage_task,
        "alert_storage_task",
        4096,
        NULL,
        5,
        &s_alert_task);

    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "Impossible de créer la tâche alert_storage_task");
        s_alert_task = NULL;
        return;
    }

    // Enregistrement du callback
    alert_register_callback(sd_on_alert_event);

    ESP_LOGI(TAG, "Callback SD enregistré");
}

/* =========================================================
   LOAD : recharge l’historique depuis SD
   ========================================================= */
void alert_storage_load(void)
{
    FILE *f = fopen(log_path, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "Aucun fichier d'historique trouvé (%s)", log_path);
        return;
    }

    alert_clear_all();

    char line[256];
    int line_count = 0;

    while (fgets(line, sizeof(line), f))
    {

        long timestamp = 0;
        char name[64] = {0};
        int activated = 0;

        sscanf(line,
               "{ \"timestamp\": %ld, \"name\": \"%63[^\"]\", \"activated\": %d }",
               &timestamp, name, &activated);

        if (timestamp == 0 || strlen(name) == 0)
            continue;

        alert_log_t log;
        log.timestamp = timestamp;
        strncpy(log.name, name, sizeof(log.name));
        log.activated = activated;

        alert_push_history(&log);

        if (++line_count > MAX_LOG_LINES)
        {
            ESP_LOGW(TAG, "Fichier trop long → rotation");
            fclose(f);
            alert_storage_rotate();
            return;
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Historique SD chargé (%d lignes)", line_count);
}
