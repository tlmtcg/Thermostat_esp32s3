#include "led_ctrl.h"

#include "alert_manager.h"
#include "esp_log.h"
#include "led_db.h"
#include "led_driver.h"
#include "led_task.h"

static const char *TAG = "LED_CTRL";

led_mode_t current_bg_mode = LED_MODE_FIXED;
led_color_t current_bg_color = {0, 0, 0};
int current_bg_speed = 1000;

esp_err_t led_init(void)
{
    ESP_LOGI(TAG, "Initialisation du module LED...");

    esp_err_t ret = led_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Echec init driver LED: %s", esp_err_to_name(ret));
        return ret;
    }

    led_db_init();
    alert_manager_init();

    return ESP_OK;
}

void led_stop(void)
{
    current_bg_mode = LED_MODE_FIXED;
    current_bg_color = (led_color_t){0, 0, 0};
    current_bg_speed = 1000;

    led_driver_clear();
    led_driver_refresh();

    ESP_LOGI(TAG, "LED stoppee et eteinte.");
}

void led_set_background(led_mode_t mode, led_color_t color, int speed_ms)
{
    current_bg_mode = mode;
    current_bg_color = color;
    current_bg_speed = speed_ms;

    ESP_LOGI(
        TAG,
        "Background mis a jour: mode=%d rgb=%d,%d,%d speed=%dms",
        mode,
        color.r,
        color.g,
        color.b,
        speed_ms);
}
