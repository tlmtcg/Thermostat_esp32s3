#include "alert_manager.h"
#include "led_db.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include "sdkconfig.h"

static const char *TAG = "ALERT_MANAGER";

/* =========================================================
   STOCKAGE INTERNE
   ========================================================= */

alert_log_t alert_history[CONFIG_MAX_ALERT_LOGS];
static int s_history_count = 0;

static int s_active_alarm_indices[CONFIG_MAX_ACTIVE_ALERTS];
static int s_active_count = 0;

static alert_callback_t s_callback = NULL;

/* =========================================================
   PARAMÈTRE DE TRI
   ========================================================= */

static alert_order_t s_order = ALERT_ORDER_ACTIVATION;

void alert_set_order(alert_order_t order)
{
    s_order = order;
}

alert_order_t alert_get_order(void)
{
    return s_order;
}

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

    if (s_history_count >= CONFIG_MAX_ALERT_LOGS)
    {
        memmove(&alert_history[0], &alert_history[1],
                sizeof(alert_log_t) * (CONFIG_MAX_ALERT_LOGS - 1));
        s_history_count = CONFIG_MAX_ALERT_LOGS - 1;
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
        return true;

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
   API : COMPTE, LISTES, TRI
   ========================================================= */

int alert_get_active_count(void)
{
    return s_active_count;
}

/**
 * Renvoie une liste TRIÉE selon le mode choisi.
 */
const int *alert_get_active_list(void)
{
    static int sorted[CONFIG_MAX_ACTIVE_ALERTS];

    for (int i = 0; i < s_active_count; i++)
        sorted[i] = s_active_alarm_indices[i];

    if (s_order == ALERT_ORDER_SEVERITY)
    {
        for (int i = 0; i < s_active_count - 1; i++)
        {
            for (int j = i + 1; j < s_active_count; j++)
            {
                stored_alarm_t *ai = led_db_internal_get_alarm_by_idx(sorted[i]);
                stored_alarm_t *aj = led_db_internal_get_alarm_by_idx(sorted[j]);

                int sevi = get_alarm_severity(ai->name);
                int sevj = get_alarm_severity(aj->name);

                if (sevj > sevi)
                {
                    int tmp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = tmp;
                }
            }
        }
    }

    return sorted;
}

/* =========================================================
   SÉVÉRITÉ / SANTÉ / PRIORITÉ
   ========================================================= */

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

/* =========================================================
   HISTORIQUE
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
