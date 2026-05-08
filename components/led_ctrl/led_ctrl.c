#include "led_ctrl.h"
#include "led_driver.h"
#include "led_db.h"
#include "led_storage.h"
#include "alert_manager.h"
#include "led_task.h"
#include "esp_log.h"

static const char *TAG = "LED_CTRL";

// ============================================================================
// ÉTAT GLOBAL DU FOND (utilisé par led_task.c)
// ============================================================================
led_mode_t current_bg_mode = LED_MODE_FIXED;
led_color_t current_bg_color = {0, 0, 0};
int current_bg_speed = 1000;

// ============================================================================
// INITIALISATION
// ============================================================================
esp_err_t led_init(void)
{
    ESP_LOGI(TAG, "Initialisation du module LED (pipeline A)...");

    // 1. Stockage
    esp_err_t ret = led_storage_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec init stockage: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Driver LED
    ret = led_driver_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec init driver LED: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Base de données LED
    led_db_init();

    // 4. Gestionnaire d’alertes
    alert_manager_init();

    // 5. Tâche LED
    ret = led_task_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec démarrage tâche LED");
        return ret;
    }

    return ESP_OK;
}

// ============================================================================
// STOP LED
// ============================================================================
void led_stop(void)
{
    current_bg_mode = LED_MODE_FIXED;
    current_bg_color = (led_color_t){0, 0, 0};
    current_bg_speed = 1000;

    led_driver_clear();
    led_clear_alarms();

    led_driver_refresh();

    ESP_LOGI(TAG, "LED stoppée et éteinte.");
}

// ============================================================================
// BACKGROUND (FOND)
// ============================================================================
void led_set_background(led_mode_t mode, led_color_t color, int speed_ms)
{
    current_bg_mode = mode;
    current_bg_color = color;
    current_bg_speed = speed_ms;

    ESP_LOGI(TAG, "Background mis à jour: Mode=%d, RGB=%d,%d,%d, speed=%dms",
             mode, color.r, color.g, color.b, speed_ms);
}

// ============================================================================
// API LEGACY (pipeline B) — conservée mais NEUTRE en pipeline A
// ============================================================================
void led_add_alarm(int nb_blinks, led_color_t color)
{
    ESP_LOGW(TAG, "led_add_alarm() obsolète en pipeline A. Utiliser alert_add(name).");
    (void)nb_blinks;
    (void)color;
}

void led_clear_alarms(void)
{
    ESP_LOGW(TAG, "led_clear_alarms() redirigé vers alert_clear_all()");
    alert_clear_all();
}

void led_set_effect(led_mode_t mode, led_color_t color, int count, int speed_ms)
{
    (void)count;
    led_set_background(mode, color, speed_ms);
}

// ============================================================================
// DÉLÉGATION DB
// ============================================================================
void led_db_add_info(const char *name, led_color_t color)
{
    led_db_internal_add_info(name, color);
}

void led_db_add_alarm(const char *name, int blinks, led_color_t color)
{
    led_db_internal_add_alarm(name, blinks, color);
}

void led_db_delete_by_name(const char *name)
{
    led_db_internal_delete_by_name(name);
}

void led_db_print_status(void)
{
    led_db_internal_print_status();
}

int led_db_get_info_count(void)
{
    return led_db_internal_get_info_count();
}

int led_db_get_alarm_count(void)
{
    return led_db_internal_get_alarm_count();
}

stored_info_t *led_db_get_info_by_idx(int idx)
{
    return led_db_internal_get_info_by_idx(idx);
}

stored_alarm_t *led_db_get_alarm_by_idx(int idx)
{
    return led_db_internal_get_alarm_by_idx(idx);
}

void led_db_simulate(int info_idx, int alarm_idx)
{
    led_db_internal_simulate(info_idx, alarm_idx);
}
