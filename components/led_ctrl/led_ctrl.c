// components/led/led_ctrl.c
#include "led_ctrl.h"
#include "led_driver.h"
#include "led_db.h"       // Header pour les fonctions de la base de données
#include "led_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <math.h>
#include "alert_manager.h"

static const char *TAG = "LED_CTRL";

// Variables globales (état actuel)
static led_mode_t current_bg_mode = LED_MODE_FIXED;
static led_color_t current_bg_color = {0, 0, 0};
static QueueHandle_t alarm_queue = NULL;

// On définit une période fixe pour ne pas modifier la structure stored_alarm_t
#define FIXED_BLINK_DELAY_MS 250 

static void led_task(void *pvParameters) {
    uint32_t step = 0;
    led_color_t last_displayed_color = {0, 0, 0}; 
    bool first_run = true;

    while (1) {
        // 1. On récupère l'index de l'alarme prioritaire
        int alarm_idx = alert_get_top_priority();
        
        // 2. On calcule la couleur de fond actuelle
        led_color_t bg_color = {0, 0, 0};
        if (current_bg_mode == LED_MODE_FIXED) {
            bg_color = current_bg_color;
        } else if (current_bg_mode == LED_MODE_BREATH) {
            float brightness = (sinf(step * 0.05f - 1.57f) + 1.0f) / 2.0f;
            bg_color.r = (uint8_t)(current_bg_color.r * brightness);
            bg_color.g = (uint8_t)(current_bg_color.g * brightness);
            bg_color.b = (uint8_t)(current_bg_color.b * brightness);
            step++;
        }

        // 3. Logique d'affichage
        if (alarm_idx != -1) {
            // MODE ALARME
            stored_alarm_t *alarm = led_db_get_alarm_by_idx(alarm_idx);
            if (alarm) {
                for (int i = 0; i < alarm->blinks; i++) {
                    // Phase ON
                    led_driver_set_pixel(alarm->color);
                    led_driver_refresh();
                    vTaskDelay(pdMS_TO_TICKS(FIXED_BLINK_DELAY_MS));

                    // Phase OFF (repas sur le fond)
                    led_driver_set_pixel(bg_color);
                    led_driver_refresh();
                    vTaskDelay(pdMS_TO_TICKS(FIXED_BLINK_DELAY_MS));
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } 
        else {
            // MODE NORMAL
            if (bg_color.r != last_displayed_color.r || 
                bg_color.g != last_displayed_color.g || 
                bg_color.b != last_displayed_color.b || first_run) {
                
                led_driver_set_pixel(bg_color);
                led_driver_refresh();
                last_displayed_color = bg_color;
                first_run = false;
            }
            vTaskDelay(pdMS_TO_TICKS(current_bg_mode == LED_MODE_BREATH ? 20 : 100));
        }
    }
}

esp_err_t led_init(void) {
    // Initialiser le stockage
    esp_err_t ret = led_storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec de l'initialisation du stockage: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialiser le driver LED
    ret = led_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec de l'initialisation du driver LED: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialiser la base de données
    led_db_init();

    // Créer la file d'attente pour les alarmes
    alarm_queue = xQueueCreate(10, sizeof(alarm_event_t));
    if (!alarm_queue) {
        ESP_LOGE(TAG, "Échec de la création de la file d'attente des alarmes");
        return ESP_ERR_NO_MEM;
    }

    // Démarrer la tâche LED
    BaseType_t task_ret = xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Échec de la création de la tâche LED");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void led_stop(void) {
    if (alarm_queue) {
        xQueueReset(alarm_queue);
    }
    current_bg_mode = LED_MODE_FIXED;
    current_bg_color = (led_color_t){0, 0, 0};
    led_driver_clear();
    led_driver_refresh();
    ESP_LOGI(TAG, "LED stoppée et éteinte.");
}

void led_set_background(led_mode_t mode, led_color_t color, int speed_ms) {
    current_bg_mode = mode;
    current_bg_color = color;
    ESP_LOGI(TAG, "Background mis à jour: Mode=%d, RGB=%d,%d,%d", mode, color.r, color.g, color.b);
}

void led_add_alarm(int nb_blinks, led_color_t color) {
    if (alarm_queue) {
        alarm_event_t evt = {.blinks = nb_blinks, .color = color};
        if (xQueueSend(alarm_queue, &evt, 0) != pdPASS) {
            ESP_LOGW(TAG, "Queue d'alarme pleine");
        }
    } else {
        ESP_LOGE(TAG, "Erreur: alarm_queue non initialisée");
    }
}

void led_clear_alarms(void) {
    if (alarm_queue) {
        xQueueReset(alarm_queue);
    }
    led_stop();
}

void led_set_effect(led_mode_t mode, led_color_t color, int count, int speed_ms) {
    led_set_background(mode, color, speed_ms);
}

// --- Fonctions de délégation vers led_db.c ---
// Ces fonctions appellent directement les implémentations dans led_db.c
// (Pas de redéfinition !)

void led_db_add_info(const char *name, led_color_t color) {
    // Appel direct à la fonction dans led_db.c
    extern void led_db_internal_add_info(const char *name, led_color_t color);
    led_db_internal_add_info(name, color);
}

void led_db_add_alarm(const char *name, int blinks, led_color_t color) {
    extern void led_db_internal_add_alarm(const char *name, int blinks, led_color_t color);
    led_db_internal_add_alarm(name, blinks, color);
}

void led_db_delete_by_name(const char *name) {
    extern void led_db_internal_delete_by_name(const char *name);
    led_db_internal_delete_by_name(name);
}

void led_db_print_status(void) {
    extern void led_db_internal_print_status(void);
    led_db_internal_print_status();
}

int led_db_get_info_count(void) {
    extern int led_db_internal_get_info_count(void);
    return led_db_internal_get_info_count();
}

int led_db_get_alarm_count(void) {
    extern int led_db_internal_get_alarm_count(void);
    return led_db_internal_get_alarm_count();
}

stored_info_t *led_db_get_info_by_idx(int idx) {
    extern stored_info_t *led_db_internal_get_info_by_idx(int idx);
    return led_db_internal_get_info_by_idx(idx);
}

stored_alarm_t *led_db_get_alarm_by_idx(int idx) {
    extern stored_alarm_t *led_db_internal_get_alarm_by_idx(int idx);
    return led_db_internal_get_alarm_by_idx(idx);
}

void led_db_simulate(int info_idx, int alarm_idx) {
    extern void led_db_internal_simulate(int info_idx, int alarm_idx);
    led_db_internal_simulate(info_idx, alarm_idx);
}