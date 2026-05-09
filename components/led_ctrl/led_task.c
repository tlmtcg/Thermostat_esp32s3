#include "led_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_ctrl.h"
#include "led_driver.h"
#include "led_db.h"
#include "alert_manager.h"
#include "task_manager.h"
#include <math.h>

static const char *TAG = "LED_TASK";

#define FIXED_BLINK_DELAY_MS 250

// Variables globales définies dans led_ctrl.c
extern led_mode_t current_bg_mode;
extern led_color_t current_bg_color;
extern int current_bg_speed;

/* =========================================================
   TÂCHE LED
   ========================================================= */
void led_task(void *pvParameters)
{
    uint32_t step = 0;
    led_color_t last_displayed_color = {0, 0, 0};
    bool first_run = true;
    // On récupère le handle via le getter
    EventGroupHandle_t ev_group = task_manager_get_event_group();

    while (1)
    {
        // Cette ligne bloque la tâche si l'interrupteur est sur OFF
        xEventGroupWaitBits(ev_group, BIT_LED_EN, pdFALSE, pdTRUE, portMAX_DELAY);

        /* =====================================================
           1. Récupération des alarmes actives (triées)
           ===================================================== */
        int count = alert_get_active_count();
        const int *list = alert_get_active_list();

        /* =====================================================
           2. Calcul du fond (fixed ou breath)
           ===================================================== */
        led_color_t bg = {0, 0, 0};

        if (current_bg_mode == LED_MODE_FIXED)
        {
            bg = current_bg_color;
        }
        else if (current_bg_mode == LED_MODE_BREATH)
        {
            float speed_factor = (current_bg_speed <= 0)
                                     ? 1.0f
                                     : (1000.0f / (float)current_bg_speed);

            float brightness = (sinf(step * 0.05f * speed_factor - 1.57f) + 1.0f) / 2.0f;

            bg.r = (uint8_t)(current_bg_color.r * brightness);
            bg.g = (uint8_t)(current_bg_color.g * brightness);
            bg.b = (uint8_t)(current_bg_color.b * brightness);

            step++;
        }

        /* =====================================================
           3. MODE ALARME : afficher TOUTES les alarmes actives
           ===================================================== */
        if (count > 0)
        {
            for (int i = 0; i < count; i++)
            {
                int alarm_idx = list[i];
                stored_alarm_t *alarm = led_db_get_alarm_by_idx(alarm_idx);
                if (!alarm)
                    continue;

                // ESP_LOGI(TAG, "Blink alarme '%s' (%d blinks)",
                //          alarm->name, alarm->blinks);

                for (int b = 0; b < alarm->blinks; b++)
                {
                    // ON
                    led_driver_set_pixel(alarm->color);
                    led_driver_refresh();
                    vTaskDelay(pdMS_TO_TICKS(FIXED_BLINK_DELAY_MS));

                    // OFF = fond
                    led_driver_set_pixel(bg);
                    led_driver_refresh();
                    vTaskDelay(pdMS_TO_TICKS(FIXED_BLINK_DELAY_MS));
                }

                // Petite pause entre deux alarmes
                vTaskDelay(pdMS_TO_TICKS(500));
            }

            // Pause avant de recommencer la séquence
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            /* =====================================================
               4. MODE NORMAL : afficher le fond
               ===================================================== */
            if (first_run ||
                bg.r != last_displayed_color.r ||
                bg.g != last_displayed_color.g ||
                bg.b != last_displayed_color.b)
            {
                led_driver_set_pixel(bg);
                led_driver_refresh();
                last_displayed_color = bg;
                first_run = false;
            }

            vTaskDelay(pdMS_TO_TICKS(
                current_bg_mode == LED_MODE_BREATH ? 20 : 100));
        }
    }
}

/* =========================================================
   DÉMARRAGE DE LA TÂCHE
   ========================================================= 
   La tache est créée dans le task_manager, mais on fournit une
   fonction de démarrage dédiée pour faciliter l'intégration et
   la gestion des erreurs.
*/
esp_err_t led_task_start(void)
{
    ESP_LOGI(TAG, "Démarrage de la tâche LED (pipeline A)");

    BaseType_t ret = xTaskCreate(
        led_task,
        "led_task",
        4096,
        NULL,
        5,
        NULL);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Échec de la création de la tâche LED");
        return ESP_FAIL;
    }

    return ESP_OK;
}
