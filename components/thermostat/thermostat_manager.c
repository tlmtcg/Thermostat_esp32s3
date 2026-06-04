#include "thermostat.h"
#include "esp_log.h"
#include "relay.h"
#include "math.h"
#include <prediction_engine.h>
#include "config_runtime.h"
#include "time.h"
#include "sht31.h"

static const char *TAG = "HEAT";

// Déclaration de la structure globale originale de votre projet
extern thermostat_runtime_t g_thermostat_runtime;

// Variables internes au module
float g_critical_fallback_duty = 0.0f;
bool g_critical_active = false;

// Définition du sous-cycle de régulation (2 heures)
#define SUB_CYCLE_DURATION_SEC 7200

// Prototypes
bool critical_compute_fallback_heating(float ext_temp, bool ext_temp_valide, float *out_duty_percent);

// ============================================================
//  Watchdog thermique (stub)
// ============================================================
static void thermal_watchdog(void)
{
    // ESP_LOGW("WATCHDOG", "Watchdog thermique : fonction non implémentée");
}

// ============================================================
//  Logique principale de décision du chauffage
// ============================================================
void must_heat(void)
{

    thermal_watchdog();

    // Récupération directe via le pointeur de la structure globale originale
    thermostat_runtime_t *rt = &g_thermostat_runtime;
    const thermal_runtime_t tr = g_thermal_runtime;

    thermostat_config_t cfg;
    thermostat_get_config(&cfg);

    ESP_LOGI(TAG, "=== must_heat() | In: %.2f°C | Ext: %.2f°C | Mode: %d ===", rt->temperature, rt->temp_ext, cfg.mode);

    // =========================================================
    // PRIORITY 1. GESTION DU MODE CRITIQUE (Capteur Intérieur HS)
    // =========================================================

    bool int_temp_valide = sht31_get_runtime()->valid;

    ESP_LOGI(TAG,
             "TempValid=%d | Temp=%.2f | Ext=%.2f | Critical=%d",
             sht31_get_runtime()->valid,
             rt->temperature,
             rt->temp_ext,
             rt->critical_active);

    if (!int_temp_valide)
    {
        ESP_LOGE(TAG,
                 ">>> MODE CRITIQUE ACTIVE <<<");

        g_critical_active = true;
        rt->critical_active = true;

        float ext_temp = rt->temp_ext;

        bool ext_temp_valide =
            !isnan(ext_temp) &&
            (ext_temp > -40.0f) &&
            (ext_temp < 60.0f);

        if (!ext_temp_valide)
        {
            ESP_LOGW(TAG,
                     "Température extérieure invalide");
        }

        bool requiert_chauffage =
            critical_compute_fallback_heating(
                ext_temp,
                ext_temp_valide,
                &g_critical_fallback_duty);

        rt->fallback_duty =
            (uint32_t)g_critical_fallback_duty;

        if (requiert_chauffage)
            relay_on();
        else
            relay_off();

        return;
    }

    // =========================================================
    // SORTIE AUTOMATIQUE DU MODE CRITIQUE
    // =========================================================
    g_critical_active = false;
    rt->critical_active = false;
    g_critical_fallback_duty = 0.0f;
    rt->fallback_duty = 0;

    // =========================================================
    // PRIORITY 2. LOGIQUE DE RÉGULATION NORMALE
    // =========================================================
    float hysteresis_active = cfg.hysteresis;
    float calibration_active = cfg.calibration;

    if (hysteresis_active < 0.05f)
        hysteresis_active = 0.20f;

    float consigne_active = rt->effective_consigne;
    if (isnan(consigne_active) || consigne_active < 5.0f || consigne_active > 45.0f)
        consigne_active = 19.0f;

    float temp_calibree = rt->temperature + calibration_active;
    time_t now = time(NULL);

    // =========================
    // MODE MANUEL
    // =========================
    if (cfg.mode == THERMOSTAT_MODE_MANUAL)
    {
        float seuil_allumage = consigne_active - hysteresis_active;
        float seuil_extinction = consigne_active + hysteresis_active;

        if (temp_calibree <= seuil_allumage)
        {
            relay_on();
        }
        else if (temp_calibree >= seuil_extinction)
        {
            relay_off();
        }
        return;
    }

    // =========================
    // MODE AUTO + 2R2C
    // =========================
    if (rt->enable_2r2c)
    {
        if (cfg.mode == THERMOSTAT_MODE_AUTO)
        {
            if (tr.time_to_reach < 0)
            {
                relay_on();
                return;
            }

            if (tr.start_heating_at > 0 && now >= tr.start_heating_at)
            {
                relay_on();
                return;
            }
        }
    }

    // =========================
    // HYSTÉRÉSIS STANDARD
    // =========================
    float seuil_allumage = consigne_active - hysteresis_active;
    float seuil_extinction = consigne_active + hysteresis_active;

    if (temp_calibree <= seuil_allumage)
    {
        relay_on();
    }
    else if (temp_calibree >= seuil_extinction)
    {
        relay_off();
    }
}

// ============================================================
//  Calcul de la puissance de repli temporel (PWM)
// ============================================================
bool critical_compute_fallback_heating(float ext_temp,
                                       bool ext_temp_valide,
                                       float *out_duty_percent)
{
    uint32_t current_time_sec = (uint32_t)time(NULL);
    thermostat_runtime_t *rt = &g_thermostat_runtime;

    // =====================================================
    // Détermination de la consigne cible
    // =====================================================
    float consigne_cible = rt->effective_consigne;

    if (isnan(consigne_cible) ||
        consigne_cible < 5.0f ||
        consigne_cible > 45.0f)
    {
        consigne_cible = 19.0f;
    }

    float duty_cycle_percent = 0.0f;

    // =====================================================
    // Cas normal : température extérieure valide
    // =====================================================
    if (ext_temp_valide)
    {
        if (ext_temp < consigne_cible)
        {
            // Température extérieure extrême configurable
            float extreme_temp = g_cfg.extreme_ext_temp;

            float plage_thermique =
                consigne_cible - extreme_temp;

            if (plage_thermique > 0.0f)
            {
                // Loi linéaire :
                // ext_temp = consigne -> 0%
                // ext_temp = extreme_temp -> 100%
                duty_cycle_percent =
                    ((consigne_cible - ext_temp) /
                     plage_thermique) *
                    100.0f;
            }
            else
            {
                duty_cycle_percent = 100.0f;
            }
        }
        else
        {
            // Extérieur plus chaud que la consigne
            duty_cycle_percent = 0.0f;
        }
    }
    else
    {
        // =================================================
        // Double panne :
        // capteur intérieur HS + température extérieure HS
        // =================================================

        duty_cycle_percent =
            (float)g_cfg.fallback_duty_percent;

        ESP_LOGW(TAG,
                 "Double panne capteurs -> duty fixe secours %.1f%%",
                 duty_cycle_percent);
    }

    // =====================================================
    // Sécurisation du ratio
    // =====================================================
    if (duty_cycle_percent > 100.0f)
        duty_cycle_percent = 100.0f;

    if (duty_cycle_percent < 0.0f)
        duty_cycle_percent = 0.0f;

    // Export pour JSON / supervision
    if (out_duty_percent)
    {
        *out_duty_percent = duty_cycle_percent;
    }

    // =====================================================
    // Conversion Duty (%) -> Temps ON dans un cycle
    // =====================================================
    uint32_t progress_in_sub_cycle =
        current_time_sec % SUB_CYCLE_DURATION_SEC;

    uint32_t heating_time_in_sub_cycle =
        (uint32_t)((duty_cycle_percent / 100.0f) *
                   SUB_CYCLE_DURATION_SEC);

    // =====================================================
    // Décision ON/OFF
    // =====================================================
    bool requiert_chauffage = false;

    // On ignore les durées ON inférieures à 5 minutes
    if (heating_time_in_sub_cycle >= 300)
    {
        // Si OFF serait inférieur à 5 minutes,
        // on reste allumé en permanence
        if ((SUB_CYCLE_DURATION_SEC -
             heating_time_in_sub_cycle) < 300)
        {
            requiert_chauffage = true;
        }
        else
        {
            requiert_chauffage =
                (progress_in_sub_cycle <
                 heating_time_in_sub_cycle);
        }
    }

    ESP_LOGI(TAG,
             "[PWM Secours] Ext=%.1f°C | Duty=%.1f%% | "
             "Progress=%u/%us | ON=%us | Heat=%d",
             ext_temp,
             duty_cycle_percent,
             progress_in_sub_cycle,
             SUB_CYCLE_DURATION_SEC,
             heating_time_in_sub_cycle,
             requiert_chauffage);

    return requiert_chauffage;
}
