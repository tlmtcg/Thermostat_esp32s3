#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <stdbool.h>
#include <time.h>
#include "led_db.h"

/* =========================================================
   TYPES
   ========================================================= */

typedef enum
{
    HEALTH_OK = 0,
    HEALTH_WARNING,
    HEALTH_CRITICAL,
    HEALTH_HARDWARE_FAIL
} board_health_t;

typedef enum
{
    ALERT_EVENT_ADDED,
    ALERT_EVENT_REMOVED
} alert_event_t;

typedef struct
{
    char name[32];
    time_t timestamp;
    bool activated; // true = START, false = STOP
} alert_log_t;

/* =========================================================
   CALLBACK
   ========================================================= */

typedef void (*alert_callback_t)(alert_event_t evt, const alert_log_t *log);

/* =========================================================
   VARIABLES EXTERNES
   ========================================================= */

// Historique global (utilisé par webserver, serial, jeedom…)
extern alert_log_t alert_history[CONFIG_CONFIG_MAX_ALERT_LOGS];

/* =========================================================
   API PRINCIPALE
   ========================================================= */

void alert_manager_init(void);

/* Ajout / suppression d’alarme */
bool alert_add(const char *name);
bool alert_remove(const char *name);

/* Comptage */
int alert_get_count(void);
int alert_get_active_count(void);

/* Liste des alarmes actives (indices led_db) */
const int *alert_get_active_list(void);

/* Liste sous forme de chaîne "A,B,C" */
const char *get_alarms_list(void);

/* Récupération d’une entrée d’historique */
const alert_log_t *alert_get_by_index(int i);

/* Historique console */
void alert_get_history(void);

/* Historique (utilisé par alert_storage_load) */
void alert_push_history(const alert_log_t *log);

/* Nettoyage complet */
void alert_clear_all(void);

/* =========================================================
   SANTÉ / PRIORITÉ / SÉVÉRITÉ
   ========================================================= */

/* Renvoie l’index led_db de l’alarme la plus prioritaire */
int alert_get_top_priority(void);

/* Convertit une alarme en sévérité (0–3) */
int get_alarm_severity(const char *name);

/* Renvoie l’état de santé global */
board_health_t alert_get_board_health(void);

/* Convertit en texte */
const char *alert_health_to_str(board_health_t health);

/* =========================================================
   WEBSERVER / JEEDOM
   ========================================================= */

/* Remplit un tableau de logs actifs */
int get_active_alarms(alert_log_t *out, int max);

/* =========================================================
   CALLBACK
   ========================================================= */

void alert_register_callback(alert_callback_t cb);

#endif
