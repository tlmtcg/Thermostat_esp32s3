#include "critical.h"
#include <time.h>
#include "esp_log.h"
#include "config_runtime.h" // Contient g_cfg (secu_cycle_duration_sec, fallback_duty_percent, extreme_ext_temp)

static const char *TAG = "CRITICAL_MODE";

bool critical_compute_fallback_heating(float ext_temp, bool ext_temp_valide)
{
    uint32_t current_time_sec = (uint32_t)time(NULL);
    
    // Protection contre une configuration invalide (évite la division par zéro)
    uint32_t cycle_duration = g_cfg.secu_cycle_duration_sec;
    if (cycle_duration == 0) {
        cycle_duration = 7200; // Par défaut : 2 heures
    }

    uint32_t cycle_progress_sec = current_time_sec % cycle_duration;
    float duty_cycle_percent = 0.0f;
    const float base_consigne = 5.0f; // CONSIGNE_MIN_ECO

    if (ext_temp_valide)
    {
        // Calcul du % de chauffe indexé sur le froid extérieur
        // Si ext_temp >= base_consigne -> 0%. Si ext_temp <= extreme_ext_temp -> 100%
        if (ext_temp < base_consigne)
        {
            float plage = base_consigne - g_cfg.extreme_ext_temp;
            if (plage > 0.0f) {
                duty_cycle_percent = ((base_consigne - ext_temp) / plage) * 100.0f;
            } else {
                duty_cycle_percent = 100.0f;
            }
            
            // Bornage strict entre 0 et 100%
            if (duty_cycle_percent > 100.0f) duty_cycle_percent = 100.0f;
            if (duty_cycle_percent < 0.0f)   duty_cycle_percent = 0.0f;
        }
    }
    else
    {
        // Double panne (Intérieur + Extérieur) -> Mode dégradé fixe basé sur l'interface web
        duty_cycle_percent = (float)g_cfg.fallback_duty_percent;
    }

    // Calcul du temps de marche effectif en secondes à l'intérieur de la période
    uint32_t max_heating_time_sec = (uint32_t)((duty_cycle_percent / 100.0f) * cycle_duration);

    // Détermination de l'état du relais
    bool requiert_chauffage = (cycle_progress_sec < max_heating_time_sec) && (max_heating_time_sec > 0);
    
    // Log d'information espacé toutes les 10 minutes
    static uint32_t last_log_time = 0;
    if (current_time_sec - last_log_time >= 600)
    {
        ESP_LOGW(TAG, "Secours actif - Ext: %s (%.1f C) -> Cycle Progress: %lus/%lus | Chauffe: %.0f%% (%s)",
                 ext_temp_valide ? "OK" : "HS", 
                 ext_temp, 
                 (unsigned long)cycle_progress_sec, 
                 (unsigned long)cycle_duration,
                 duty_cycle_percent, 
                 requiert_chauffage ? "ON" : "OFF");
        last_log_time = current_time_sec;
    }

    return requiert_chauffage;
}