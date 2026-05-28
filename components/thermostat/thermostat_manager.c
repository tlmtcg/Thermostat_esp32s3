#include "thermostat.h"
#include "esp_log.h"
#include "relay.h"
#include "math.h"
#include <prediction_engine.h>
#include "time.h"
#include "thermostat.h"

static const char *TAG = "HEAT";
// #define THERMOSTAT_DEBUG 0

// ============================================================
//  Sélection du mode de décision
//  0 = Hystérésis simple
//  1 = Pilotage par modèle 2R2C (prédictif)
// ============================================================
// #define USE_2R2C 1

// ============================================================
//  Watchdog thermique (stub pour l'instant)
// ============================================================
static void thermal_watchdog(void)
{
    // ESP_LOGW("WATCHDOG", "Watchdog thermique : fonction non implémentée");
}

void must_heat(void)
{
    thermal_watchdog();

    const thermostat_runtime_t rt = *thermostat_get_runtime();
    const thermal_runtime_t tr = g_thermal_runtime;

    thermostat_config_t cfg;
    thermostat_get_config(&cfg);

    float hysteresis_active = cfg.hysteresis;
    float calibration_active = cfg.calibration;

    if (hysteresis_active < 0.05f)
        hysteresis_active = 0.20f;

    float consigne_active = rt.effective_consigne;
    if (isnan(consigne_active) || consigne_active < 5.0f || consigne_active > 45.0f)
        consigne_active = 19.0f;

    float temp_calibree = rt.temperature + calibration_active;
    time_t now = time(NULL);

#ifdef THERMOSTAT_DEBUG
    ESP_LOGI(TAG,
             "[STATE] mode=%d temp=%.2f consigne=%.2f hyst=%.2f t2r=%.0f start_at=%lld now=%lld",
             cfg.mode,
             temp_calibree,
             consigne_active,
             hysteresis_active,
             tr.time_to_reach,
             (long long)tr.start_heating_at,
             (long long)now);
#endif
    // =========================
    // MODE MANUEL
    // =========================
    if (cfg.mode == THERMOSTAT_MODE_MANUAL)
    {
        float seuil_allumage = consigne_active - hysteresis_active;
        float seuil_extinction = consigne_active + hysteresis_active;

#ifdef THERMOSTAT_DEBUG
        ESP_LOGI(TAG,
                 "[MANUAL] Temp=%.2f Consigne=%.2f Bas=%.2f Haut=%.2f",
                 temp_calibree, consigne_active, seuil_allumage, seuil_extinction);
#endif

        if (temp_calibree <= seuil_allumage)
        {
#ifdef THERMOSTAT_DEBUG
            ESP_LOGI(TAG, "[MANUAL] ON (temp <= seuil bas)");
#endif
            relay_on();
        }
        else if (temp_calibree >= seuil_extinction)
        {
#ifdef THERMOSTAT_DEBUG
            ESP_LOGI(TAG, "[MANUAL] OFF (temp >= seuil haut)");
#endif
            relay_off();
        }
        else
        {
#ifdef THERMOSTAT_DEBUG
            ESP_LOGI(TAG, "[MANUAL] Zone neutre → pas de changement");
#endif
        }
        return;
    }

    // =========================
    // MODE AUTO + 2R2C
    // =========================
    if (g_thermostat_runtime.enable_2r2c)
    {
        if (cfg.mode == THERMOSTAT_MODE_AUTO)
        {
#ifdef THERMOSTAT_DEBUG
            ESP_LOGI(TAG,
                     "[2R2C] t2r=%.0f start_at=%lld now=%lld",
                     tr.time_to_reach,
                     (long long)tr.start_heating_at,
                     (long long)now);
#endif
            if (tr.time_to_reach < 0)
            {
#ifdef THERMOSTAT_DEBUG
                ESP_LOGW("HEAT", "[2R2C] Chauffage insuffisant (t2r<0) → ON");
#endif
                relay_on();
                return;
            }

            if (tr.start_heating_at > 0 && now >= tr.start_heating_at)
            {
#ifdef THERMOSTAT_DEBUG
                ESP_LOGW("HEAT", "[2R2C] Early-start atteint → ON");
#endif
                relay_on();
                return;
            }
#ifdef THERMOSTAT_DEBUG
            ESP_LOGI(TAG, "[2R2C] Pas d'action 2R2C → on passe à l'hystérésis");
#endif
        }
    }

    // =========================
    // HYSTÉRÉSIS (AUTO ou fallback)
    // =========================
    float seuil_allumage = consigne_active - hysteresis_active;
    float seuil_extinction = consigne_active + hysteresis_active;

#ifdef THERMOSTAT_DEBUG
    ESP_LOGI(TAG,
             "[HYST] Temp=%.2f Consigne=%.2f Bas=%.2f Haut=%.2f",
             temp_calibree, consigne_active, seuil_allumage, seuil_extinction);
#endif

    if (temp_calibree <= seuil_allumage)
    {
#ifdef THERMOSTAT_DEBUG
        ESP_LOGI(TAG, "[HYST] ON (temp <= seuil bas)");
#endif
        relay_on();
    }
    else if (temp_calibree >= seuil_extinction)
    {
#ifdef THERMOSTAT_DEBUG
        ESP_LOGI(TAG, "[HYST] OFF (temp >= seuil haut)");
#endif
        relay_off();
    }
    else
    {
#ifdef THERMOSTAT_DEBUG
        ESP_LOGI(TAG, "[HYST] Zone neutre → pas de changement");
#endif
    }
}
