#include "alert_manager.h"
#include "led_ctrl.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

static const char *TAG = "ALERT_MGR";

/* =========================================================
   GLOBAL STATE
   ========================================================= */

/* Pile des alarmes actives (état runtime LED + système) */
static alert_stack_t active_alerts;

/* Buffer circulaire des événements (historique START/STOP) */
alert_log_t alert_history[MAX_ALERT_LOGS];

/* Index d’écriture circulaire */
static int log_idx = 0;

/* =========================================================
   INIT MANAGER
   ========================================================= */
void alert_manager_init(void)
{
    memset(&active_alerts, 0, sizeof(alert_stack_t));

    for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
        active_alerts.alarm_indices[i] = -1;
    }

    memset(alert_history, 0, sizeof(alert_history));
    log_idx = 0;

    ESP_LOGI(TAG, "Alert manager initialisé");
}

/* =========================================================
   RECORD EVENT (HISTORIQUE CIRCULAIRE)
   ========================================================= */
static void record_alert_event(const char* name, bool activated)
{
    if (!name) return;

    alert_history[log_idx].timestamp = time(NULL);
    alert_history[log_idx].activated = activated;

    snprintf(alert_history[log_idx].name,
             sizeof(alert_history[log_idx].name),
             "%s",
             name);

    ESP_LOGI("ALERT_LOG", "Event: %s | %s",
             name,
             activated ? "START" : "STOP");

    /* avance index circulaire */
    log_idx = (log_idx + 1) % MAX_ALERT_LOGS;
}

/* =========================================================
   CHECK SI ALARME ACTIVE (IMPORTANT POUR /active)
   ========================================================= */
bool alert_is_active(const char *name)
{
    if (!name) return false;

    int idx = led_db_get_alarm_idx_by_name(name);
    if (idx == -1) return false;

    for (int i = 0; i < active_alerts.count; i++) {
        if (active_alerts.alarm_indices[i] == idx)
            return true;
    }

    return false;
}

/* =========================================================
   ADD ALERT
   ========================================================= */
bool alert_add(const char *name)
{
    int idx = led_db_get_alarm_idx_by_name(name);

    if (idx == -1 || active_alerts.count >= MAX_ACTIVE_ALERTS)
        return false;

    /* éviter doublon */
    for (int i = 0; i < active_alerts.count; i++) {
        if (active_alerts.alarm_indices[i] == idx)
            return true;
    }

    active_alerts.alarm_indices[active_alerts.count++] = idx;

    ESP_LOGI(TAG, "Alerte ajoutée: %s (Total: %d)",
             name,
             active_alerts.count);

    led_db_simulate(-1, idx);

    record_alert_event(name, true);

    return true;
}

/* =========================================================
   REMOVE ALERT
   ========================================================= */
bool alert_remove(const char *name)
{
    int idx = led_db_get_alarm_idx_by_name(name);

    if (idx == -1)
        return false;

    for (int i = 0; i < active_alerts.count; i++) {

        if (active_alerts.alarm_indices[i] == idx) {

            /* shift tableau */
            for (int j = i; j < active_alerts.count - 1; j++) {
                active_alerts.alarm_indices[j] =
                    active_alerts.alarm_indices[j + 1];
            }

            active_alerts.count--;

            ESP_LOGI(TAG, "Alerte retirée: %s (Restant: %d)",
                     name,
                     active_alerts.count);

            int next_alarm = (active_alerts.count > 0)
                             ? active_alerts.alarm_indices[active_alerts.count - 1]
                             : -1;

            led_db_simulate(-1, next_alarm);

            record_alert_event(name, false);

            return true;
        }
    }

    return false;
}

/* =========================================================
   HISTORY COUNT
   ========================================================= */
int alert_get_count(void)
{
    int count = 0;

    for (int i = 0; i < MAX_ALERT_LOGS; i++) {
        if (alert_history[i].timestamp != 0)
            count++;
    }

    return count;
}

/* =========================================================
   HISTORY ACCESS (SAFE ITERATION)
   ========================================================= */
const alert_log_t* alert_get_by_index(int i)
{
    int found = 0;

    for (int j = 0; j < MAX_ALERT_LOGS; j++) {

        if (alert_history[j].timestamp == 0)
            continue;

        if (found == i)
            return &alert_history[j];

        found++;
    }

    return NULL;
}

/* =========================================================
   CLEAR HISTORY
   ========================================================= */
void alert_clear_all(void)
{
    memset(alert_history, 0, sizeof(alert_history));
    log_idx = 0;

    ESP_LOGI(TAG, "Historique effacé");
}

/* =========================================================
   DEBUG CONSOLE
   ========================================================= */
void alert_get_history(void)
{
    printf("\n--- Historique des Alarmes ---\n");

    for (int i = 0; i < MAX_ALERT_LOGS; i++) {

        if (alert_history[i].timestamp == 0)
            continue;

        struct tm timeinfo;
        localtime_r(&alert_history[i].timestamp, &timeinfo);

        char buf[64];
        strftime(buf, sizeof(buf), "%c", &timeinfo);

        printf("[%s] %s : %s\n",
               buf,
               alert_history[i].name,
               alert_history[i].activated ? "ACTIVÉE" : "RÉSOLUE");
    }
}

/* =========================================================
   LEGACY STRING EXPORT
   ========================================================= */
const char* get_alarms_list(void)
{
    static char buffer[1024];
    buffer[0] = '\0';

    char line[128];

    for (int i = 0; i < MAX_ALERT_LOGS; i++) {

        if (alert_history[i].timestamp == 0)
            continue;

        struct tm timeinfo;
        localtime_r(&alert_history[i].timestamp, &timeinfo);

        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%c", &timeinfo);

        snprintf(line, sizeof(line),
                 "[%s] %s : %s\n",
                 timebuf,
                 alert_history[i].name,
                 alert_history[i].activated ? "ACTIVÉE" : "RÉSOLUE");

        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
    }

    return buffer;
}

int get_active_alarms(alert_log_t *out, int max)
{
    int count = 0;

    for (int i = 0; i < MAX_ALERT_LOGS && count < max; i++) {

        if (alert_history[i].timestamp == 0)
            continue;

        // Vérifie l'état réel, pas l'historique
        if (!alert_is_active(alert_history[i].name))
            continue;

        out[count++] = alert_history[i];
    }

    return count;
}


/* =========================================================
   PRIORITY
   ========================================================= */
int alert_get_top_priority(void)
{
    if (active_alerts.count <= 0)
        return -1;

    return active_alerts.alarm_indices[active_alerts.count - 1];
}

/* =========================================================
   HEALTH SYSTEM
   ========================================================= */
board_health_t alert_get_board_health(void)
{
    if (active_alerts.count == 0)
        return HEALTH_OK;

    int max_severity = 0;

    for (int i = 0; i < active_alerts.count; i++) {

        int idx = active_alerts.alarm_indices[i];
        stored_alarm_t *alarm = led_db_get_alarm_by_idx(idx);

        if (alarm) {
            int severity = get_alarm_severity(alarm->name);

            if (severity > max_severity)
                max_severity = severity;
        }
    }

    switch (max_severity) {
        case 1: return HEALTH_WARNING;
        case 2: return HEALTH_CRITICAL;
        case 3: return HEALTH_HARDWARE_FAIL;
        default: return HEALTH_OK;
    }
}

/* =========================================================
   HEALTH STRING
   ========================================================= */
const char* alert_health_to_str(board_health_t health)
{
    switch (health) {
        case HEALTH_OK: return "OK";
        case HEALTH_WARNING: return "WARNING";
        case HEALTH_CRITICAL: return "CRITICAL";
        case HEALTH_HARDWARE_FAIL: return "HARDWARE FAIL";
        default: return "UNKNOWN";
    }
}

/* =========================================================
   SEVERITY RULES
   ========================================================= */
int get_alarm_severity(const char* name)
{
    if (!name) return 2;

    if (strcmp(name, "Panne capteur") == 0) return 3;
    if (strcmp(name, "Surchauffe") == 0) return 3;
    if (strcmp(name, "Panne wifi") == 0) return 1;
    if (strcmp(name, "Panne NTP") == 0) return 1;

    return 2;
}

int alert_get_active_count(void)
{
    return active_alerts.count;
}

const int* alert_get_active_list(void)
{
    return active_alerts.alarm_indices;
}
