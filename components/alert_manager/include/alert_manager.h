#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <stdbool.h>
#include "led_db.h"
#include <time.h>

#define MAX_ACTIVE_ALERTS 5
#define MAX_ALERT_LOGS 20

typedef struct {
    int alarm_indices[MAX_ACTIVE_ALERTS];
    int count;
} alert_stack_t;

typedef enum {
    HEALTH_OK = 0,       // Aucune alarme
    HEALTH_WARNING,      // 1 alarme (ex: Wifi déconnecté)
    HEALTH_CRITICAL,     // Plusieurs alarmes ou alarme bloquante
    HEALTH_HARDWARE_FAIL // Erreur capteur critique
} board_health_t;


typedef struct {
    char name[32];
    time_t timestamp;
    bool activated; // true si l'alarme a commencé, false si elle s'est arrêtée
} alert_log_t;

// Prototypes
void alert_manager_init(void);
bool alert_add(const char *name);
bool alert_remove(const char *name);
int  alert_get_count(void);
int alert_get_top_priority(void);


void alert_get_history(void); // Pour afficher l'historique dans la console
const char* alert_health_to_str(board_health_t health);
int get_alarm_severity(const char* name);
board_health_t alert_get_board_health(void);

#endif
