#include "thermostat.h"
#include "esp_log.h"
#include "relay.h"
#include "math.h"

static const char *TAG = "HEAT";

void must_heat(void)
{
    // ==========================================
    // 1. CONFIGURATIONS PAR DÉFAUT
    // ==========================================
    const float HYSTERESIS_DEFAUT = 0.2f;
    const float CALIBRATION_DEFAUT = 0.0f;
    const float CONSIGNE_DEFAUT = 19.0f;

    // ==========================================
    // 2. RÉCUPÉRATION DES DONNÉES
    // ==========================================
    const thermostat_runtime_t g_thermostat_runtime = *thermostat_get_runtime();

    float hysteresis_active = HYSTERESIS_DEFAUT;
    float calibration_active = CALIBRATION_DEFAUT;
    float consigne_active = CONSIGNE_DEFAUT;

    // Validation de la consigne lue
    if (!isnan(g_thermostat_runtime.effective_consigne) &&
        g_thermostat_runtime.effective_consigne >= 5.0f &&
        g_thermostat_runtime.effective_consigne <= 30.0f)
    {
        consigne_active = g_thermostat_runtime.effective_consigne;
    }

    // Récupération de la configuration stockée
    thermostat_config_t config_locale;
    if (thermostat_get_config(&config_locale) == ESP_OK)
    {
        hysteresis_active = config_locale.hysteresis;
        calibration_active = config_locale.calibration;
    }

    // Sécurité anti-valeur nulle
    if (hysteresis_active < 0.05f)
    {
        hysteresis_active = 0.20f;
    }

    // Calcul des seuils physiques réels
    float temp_calibree = g_thermostat_runtime.temperature + calibration_active;
    float seuil_allumage = consigne_active - hysteresis_active;
    float seuil_extinction = consigne_active + hysteresis_active;

    // ==========================================
    // 3. LOG DE DIAGNOSTIC (Cadencé par l'appelant)
    // ==========================================
    ESP_LOGI(TAG, "[CHECK] Temp: %.2f C, Consigne: %.2f C, Seuil Bas: %.2f C, Seuil Haut: %.2f C",
             temp_calibree, consigne_active, seuil_allumage, seuil_extinction);

    // ==========================================
    // 4. LOGIQUE DE COMMUTATION PURE (HYSTÉRÉSIS)
    // ==========================================
    if (temp_calibree <= seuil_allumage)
    {
        // La température est trop basse -> On allume obligatoirement
        relay_on();
    }
    else if (temp_calibree >= seuil_extinction)
    {
        // La température est assez haute -> On éteint obligatoirement
        relay_off();
    }
    // Entre les deux seuils, on ne fait rien : le relais reste dans son état actuel
    // et sa propre protection interne gère le hardware.
}
