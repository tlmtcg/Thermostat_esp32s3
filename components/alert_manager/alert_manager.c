#include "alert_manager.h"
#include "led_ctrl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ALERT_MGR";
static alert_stack_t active_alerts;

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
            
            return true;
        }
    }
    return false;
}

int alert_get_count(void) {
    return active_alerts.count;
}
