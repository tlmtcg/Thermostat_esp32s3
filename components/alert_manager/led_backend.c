#include "led_backend.h"
#include "alert_manager.h"
#include "led_ctrl.h"
#include "esp_log.h"

static const char* TAG = "LED_BACKEND";

/* =========================================================
   TABLE DE COULEURS / EFFETS PAR SÉVÉRITÉ
   ========================================================= */

typedef struct {
    int severity;
    led_color_t color;
    led_mode_t mode;
    int speed_ms;
} led_rule_t;

static const led_rule_t LED_RULES[] = {
    { 3, {255,   0,   0}, LED_MODE_BREATH, 800  }, // HARDWARE FAIL
    { 2, {255,  80,   0}, LED_MODE_BREATH, 1200 }, // CRITICAL
    { 1, {255, 255,   0}, LED_MODE_FIXED,  0    }, // WARNING
    { 0, {  0,  50,   0}, LED_MODE_FIXED,  0    }, // OK
};

/* =========================================================
   APPLIQUE L’ÉTAT LED EN FONCTION DES ALARMES ACTIVES
   ========================================================= */
static void led_backend_apply(void)
{
    int top = alert_get_top_priority();

    if (top < 0) {
        ESP_LOGI(TAG, "Aucune alarme active → LED OK");
        led_set_background(LED_MODE_FIXED, (led_color_t){0, 50, 0}, 0);
        return;
    }

    stored_alarm_t* alarm = led_db_get_alarm_by_idx(top);
    if (!alarm) {
        ESP_LOGW(TAG, "Alarme inconnue idx=%d", top);
        return;
    }

    int severity = get_alarm_severity(alarm->name);

    /* Cherche la règle LED correspondante */
    for (int i = 0; i < sizeof(LED_RULES)/sizeof(LED_RULES[0]); i++) {
        if (LED_RULES[i].severity == severity) {

            ESP_LOGI(TAG, "LED mise à jour : %s (sev=%d)",
                     alarm->name, severity);

            led_set_background(
                LED_RULES[i].mode,
                LED_RULES[i].color,
                LED_RULES[i].speed_ms
            );
            return;
        }
    }

    /* Fallback */
    led_set_background(LED_MODE_FIXED, (led_color_t){0, 0, 50}, 0);
}

/* =========================================================
   CALLBACK : appelé à chaque START/STOP d’alarme
   ========================================================= */
static void led_on_alert_event(alert_event_t evt, const alert_log_t* log)
{
    ESP_LOGI(TAG, "Callback LED : %s (%s)",
             log->name,
             evt == ALERT_EVENT_ADDED ? "START" : "STOP");

    led_backend_apply();
}

/* =========================================================
   INIT
   ========================================================= */
void led_backend_init(void)
{
    ESP_LOGI(TAG, "Initialisation du backend LED");

    /* S’abonne au manager */
    alert_register_callback(led_on_alert_event);

    /* Applique l’état initial */
    led_backend_apply();
}
