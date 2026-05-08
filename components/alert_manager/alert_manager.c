#include "alert_manager.h"
#include "led_db.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "ALERT_MANAGER";

/* =========================================================
   STOCKAGE INTERNE
   ========================================================= */

// Historique global (visible via extern dans le .h)
alert_log_t alert_history[CONFIG_CONFIG_MAX_ALERT_LOGS];
static int s_history_count = 0;

// Liste des alarmes actives (indices dans led_db)
static int s_active_alarm_indices[CONFIG_MAX_ACTIVE_ALERTS];
static int s_active_count = 0;

// Callback externe (LED backend, SD, etc.)
static alert_callback_t s_callback = NULL;

/* =========================================================
   OUTILS INTERNES
   ========================================================= */

static int find_active_idx_by_alarm_idx(int alarm_idx)
{
    for (int i = 0; i < s_active_count; i++)
    {
        if (s_active_alarm_indices[i] == alarm_idx)
            return i;
    }
    return -1;
}

static void push_history_internal(const alert_log_t *log)
{
    if (!log)
        return;

    if (s_history_count >= CONFIG_CONFIG_MAX_ALERT_LOGS)
    {
        memmove(&alert_history[0], &alert_history[1],
                sizeof(alert_log_t) * (CONFIG_CONFIG_MAX_ALERT_LOGS - 1));
        s_history_count = CONFIG_CONFIG_MAX_ALERT_LOGS - 1;
    }

    alert_history[s_history_count++] = *log;
}

/* =========================================================
   CALLBACK
   ========================================================= */

void alert_register_callback(alert_callback_t cb)
{
    s_callback = cb;
}

/* =========================================================
   API : AJOUT / SUPPRESSION D’ALERTE
   ========================================================= */

bool alert_add(const char *name)
{
    if (!name || !*name)
        return false;

    int alarm_idx = led_db_get_alarm_idx_by_name(name);
    if (alarm_idx < 0)
    {
        ESP_LOGW(TAG, "Alarme inconnue dans led_db: %s", name);
        return false;
    }

    if (s_active_count >= CONFIG_MAX_ACTIVE_ALERTS)
    {
        ESP_LOGW(TAG, "Trop d’alertes actives, impossible d’ajouter %s", name);
        return false;
    }

    if (find_active_idx_by_alarm_idx(alarm_idx) >= 0)
        return true; // déjà active

    s_active_alarm_indices[s_active_count++] = alarm_idx;

    alert_log_t log = {0};
    log.timestamp = time(NULL);
    strncpy(log.name, name, sizeof(log.name) - 1);
    log.activated = true;

    push_history_internal(&log);

    ESP_LOGI(TAG, "ALERTE ADD : %s", name);

    if (s_callback)
        s_callback(ALERT_EVENT_ADDED, &log);

    return true;
}

bool alert_remove(const char *name)
{
    if (!name || !*name)
        return false;

    int alarm_idx = led_db_get_alarm_idx_by_name(name);
    if (alarm_idx < 0)
        return false;

    int pos = find_active_idx_by_alarm_idx(alarm_idx);
    if (pos < 0)
        return false;

    alert_log_t log = {0};
    log.timestamp = time(NULL);
    strncpy(log.name, name, sizeof(log.name) - 1);
    log.activated = false;

    push_history_internal(&log);

    memmove(&s_active_alarm_indices[pos],
            &s_active_alarm_indices[pos + 1],
            sizeof(int) * (s_active_count - pos - 1));
    s_active_count--;

    ESP_LOGI(TAG, "ALERTE REMOVE : %s", name);

    if (s_callback)
        s_callback(ALERT_EVENT_REMOVED, &log);

    return true;
}

/* =========================================================
   API : COMPTE, LISTES, PRIORITÉ
   ========================================================= */

int alert_get_count(void)
{
    return s_active_count;
}

int alert_get_active_count(void)
{
    return s_active_count;
}

const int *alert_get_active_list(void)
{
    return s_active_alarm_indices;
}

/**
 * get_alarms_list : chaîne "ALARM1,ALARM2,..."
 */
const char *get_alarms_list()
{
    static char buf[256];
    buf[0] = '\0';

    for (int i = 0; i < s_active_count; i++)
    {
        stored_alarm_t *a = led_db_internal_get_alarm_by_idx(s_active_alarm_indices[i]);
        if (!a)
            continue;

        if (buf[0] != '\0')
            strncat(buf, ",", sizeof(buf) - strlen(buf) - 1);

        strncat(buf, a->name, sizeof(buf) - strlen(buf) - 1);
    }

    return buf;
}

/* =========================================================
   SÉVÉRITÉ / SANTÉ / PRIORITÉ
   ========================================================= */

/**
 * get_alarm_severity :
 * - 2 si nom contient "FAIL", "ERROR", "PANNE"
 * - 1 si nom contient "ATTENTE", "WAIT"
 * - 1 par défaut si alarme active
 * - 0 sinon
 */
int get_alarm_severity(const char *name)
{
    if (!name)
        return 0;

    if (strstr(name, "FAIL") || strstr(name, "ERROR") || strstr(name, "Panne"))
        return 2;

    if (strstr(name, "ATTENTE") || strstr(name, "Attente") || strstr(name, "WAIT"))
        return 1;

    return 1;
}

/**
 * alert_get_top_priority :
 * renvoie l’index led_db de l’alarme la plus sévère.
 * - -1 si aucune alarme
 */
int alert_get_top_priority(void)
{
    if (s_active_count == 0)
        return -1;

    int best_idx = -1;
    int best_sev = -1;

    for (int i = 0; i < s_active_count; i++)
    {
        stored_alarm_t *a = led_db_internal_get_alarm_by_idx(s_active_alarm_indices[i]);
        if (!a)
            continue;

        int sev = get_alarm_severity(a->name);
        if (sev > best_sev)
        {
            best_sev = sev;
            best_idx = s_active_alarm_indices[i];
        }
    }

    return best_idx;
}

const char *alert_health_to_str(board_health_t health)
{
    switch (health)
    {
    case HEALTH_OK:
        return "OK";
    case HEALTH_WARNING:
        return "WARNING";
    case HEALTH_CRITICAL:
        return "CRITICAL";
    case HEALTH_HARDWARE_FAIL:
        return "HARDWARE_FAIL";
    default:
        return "UNKNOWN";
    }
}

/**
 * Règle que tu as rappelée :
 * - HEALTH_OK si aucune alarme
 * - HEALTH_WARNING si 1 alarme de sévérité 1
 * - HEALTH_CRITICAL si plusieurs alarmes
 * - HEALTH_HARDWARE_FAIL si une alarme de sévérité 2
 */
board_health_t alert_get_board_health(void)
{
    if (s_active_count == 0)
        return HEALTH_OK;

    int max_sev = 0;

    for (int i = 0; i < s_active_count; i++)
    {
        stored_alarm_t *a = led_db_internal_get_alarm_by_idx(s_active_alarm_indices[i]);
        if (!a)
            continue;

        int sev = get_alarm_severity(a->name);
        if (sev > max_sev)
            max_sev = sev;
    }

    if (max_sev >= 2)
        return HEALTH_HARDWARE_FAIL;

    if (s_active_count == 1)
        return HEALTH_WARNING;

    return HEALTH_CRITICAL;
}

/* =========================================================
   API : HISTORIQUE
   ========================================================= */

const alert_log_t *alert_get_by_index(int i)
{
    if (i < 0 || i >= s_history_count)
        return NULL;
    return &alert_history[i];
}

void alert_push_history(const alert_log_t *log)
{
    push_history_internal(log);
}

void alert_get_history(void)
{
    ESP_LOGI(TAG, "=== HISTORIQUE D’ALERTES (%d entrées) ===", s_history_count);
    for (int i = 0; i < s_history_count; i++)
    {
        const alert_log_t *log = &alert_history[i];
        ESP_LOGI(TAG, "[%02d] %s | %ld | %s",
                 i,
                 log->name,
                 (long)log->timestamp,
                 log->activated ? "START" : "STOP");
    }
}

/* =========================================================
   CLEAR / INIT
   ========================================================= */

void alert_clear_all(void)
{
    s_active_count = 0;
    s_history_count = 0;
}

void alert_manager_init(void)
{
    ESP_LOGI(TAG, "Initialisation du gestionnaire d’alertes");
    s_active_count = 0;
    s_history_count = 0;
    s_callback = NULL;
}

int get_active_alarms(alert_log_t *out, int max)
{
    if (!out || max <= 0)
        return 0;

    int n = (s_active_count < max) ? s_active_count : max;

    for (int i = 0; i < n; i++)
    {
        stored_alarm_t *a = led_db_internal_get_alarm_by_idx(s_active_alarm_indices[i]);
        if (!a)
            continue;

        alert_log_t log = {0};
        strncpy(log.name, a->name, sizeof(log.name) - 1);
        log.timestamp = time(NULL);
        log.activated = true;

        out[i] = log;
    }

    return n;
}
