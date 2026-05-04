#include "alert_manager.h"
#include "led_ctrl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ALERT_MGR";
static alert_stack_t active_alerts;
static alert_log_t alert_history[MAX_ALERT_LOGS];
static int log_idx = 0;

// Fonction interne pour enregistrer un événement
static void record_alert_event(const char* name, bool activated) {
    snprintf(alert_history[log_idx].name, 32, "%s", name);
    alert_history[log_idx].timestamp = time(NULL); // Récupère le temps actuel (NTP ou RTC)
    alert_history[log_idx].activated = activated;

    ESP_LOGI("ALERT_LOG", "Event: %s | Status: %s", name, activated ? "START" : "STOP");
    
    log_idx = (log_idx + 1) % MAX_ALERT_LOGS; // Tampon circulaire
}

void alert_manager_init(void) {
    memset(&active_alerts, 0, sizeof(alert_stack_t));
    for(int i=0; i<MAX_ACTIVE_ALERTS; i++) active_alerts.alarm_indices[i] = -1;
}

bool alert_add(const char *name) {
    int idx = led_db_get_alarm_idx_by_name(name);
    if (idx == -1 || active_alerts.count >= MAX_ACTIVE_ALERTS) return false;

    // Éviter les doublons (si l'alarme est déjà active)
    for (int i = 0; i < active_alerts.count; i++) {
        if (active_alerts.alarm_indices[i] == idx) return true;
    }

    active_alerts.alarm_indices[active_alerts.count++] = idx;
    ESP_LOGI(TAG, "Alerte ajoutée: %s (Total: %d)", name, active_alerts.count);
    
    // On lance immédiatement la simulation de la nouvelle alerte
    led_db_simulate(-1, idx);
    record_alert_event(name, true);
    return true;
}

bool alert_remove(const char *name) {
    int idx = led_db_get_alarm_idx_by_name(name);
    if (idx == -1) return false;

    for (int i = 0; i < active_alerts.count; i++) {
        if (active_alerts.alarm_indices[i] == idx) {
            // On décale les éléments suivants pour boucher le trou
            for (int j = i; j < active_alerts.count - 1; j++) {
                active_alerts.alarm_indices[j] = active_alerts.alarm_indices[j + 1];
            }
            active_alerts.count--;
            
            ESP_LOGI(TAG, "Alerte retirée: %s (Restant: %d)", name, active_alerts.count);

            // Si il reste des alarmes, on joue la précédente (la nouvelle "top")
            // Sinon on repasse à l'ambiance par défaut (index -1 ou 0)
            int next_alarm = (active_alerts.count > 0) ? active_alerts.alarm_indices[active_alerts.count - 1] : -1;
            led_db_simulate(-1, next_alarm);

            record_alert_event(name, false);
            
            return true;
        }
    }
    return false;
}

int alert_get_count(void) {
    return active_alerts.count;
}

// Ajoute cette fonction si elle manque
int alert_get_top_priority(void) {
    if (active_alerts.count > 0) {
        // Retourne l'index de la dernière alerte ajoutée (la plus prioritaire)
        return active_alerts.alarm_indices[active_alerts.count - 1];
    }
    return -1;
}

// Fonction pour consulter les logs via la console
void alert_get_history(void) {
    printf("\n--- Historique des Alarmes ---\n");
    for (int i = 0; i < MAX_ALERT_LOGS; i++) {
        int idx = (log_idx + i) % MAX_ALERT_LOGS;
        if (alert_history[idx].timestamp == 0) continue; // Log vide

        struct tm timeinfo;
        localtime_r(&alert_history[idx].timestamp, &timeinfo);
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

        printf("[%s] %s : %s\n", strftime_buf, 
               alert_history[idx].name, 
               alert_history[idx].activated ? "ACTIVÉE" : "RÉSOLUE");
    }
}

// board_health_t alert_get_board_health(void) {
//     int count = active_alerts.count;

//     if (count == 0) {
//         return HEALTH_OK;
//     } else if (count == 1) {
//         return HEALTH_WARNING;
//     } else {
//         return HEALTH_CRITICAL;
//     }
// }

const char* alert_health_to_str(board_health_t health) {
    switch (health) {
        case HEALTH_OK:            return "PARFAIT";
        case HEALTH_WARNING:       // On pourrait aussi vérifier le nom de l'alarme ici
            return "DÉGRADÉ";
        case HEALTH_CRITICAL:      return "CRITIQUE";
        case HEALTH_HARDWARE_FAIL: return "ERREUR MATÉRIELLE";
        default:                   return "INCONNU";
    }
}

/**
 * Attribue un score de sévérité selon le nom de l'alarme
 * 0: OK, 1: Warning, 2: Critique, 3: Fatal
 */
static int get_alarm_severity(const char* name) {
    if (strcmp(name, "Panne capteur") == 0) return 3;  // Fatal: on ne peut plus réguler
    if (strcmp(name, "Surchauffe") == 0)     return 3;  // Fatal: danger thermique
    if (strcmp(name, "Panne wifi") == 0)   return 1;  // Warning: le thermostat tourne encore
    if (strcmp(name, "Panne NTP") == 0)    return 1;  // Warning: l'heure n'est pas à jour
    
    return 2; // Par défaut: Critique pour toute alarme inconnue
}

board_health_t alert_get_board_health(void) {
    if (active_alerts.count == 0) {
        return HEALTH_OK;
    }

    int max_severity = 0;

    // On parcourt toutes les alarmes actuellement dans la pile
    for (int i = 0; i < active_alerts.count; i++) {
        int idx = active_alerts.alarm_indices[i];
        stored_alarm_t *alarm = led_db_get_alarm_by_idx(idx);
        
        if (alarm) {
            int severity = get_alarm_severity(alarm->name);
            if (severity > max_severity) {
                max_severity = severity;
            }
        }
    }

    // Traduction du score en état de santé
    switch (max_severity) {
        case 1:  return HEALTH_WARNING;
        case 2:  return HEALTH_CRITICAL;
        case 3:  return HEALTH_HARDWARE_FAIL;
        default: return HEALTH_OK;
    }
}

