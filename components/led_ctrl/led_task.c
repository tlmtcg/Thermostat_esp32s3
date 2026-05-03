#include "led_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "led_strip.h"
#include "led_task.h"

static const char *TAG = "LED_TASK";

#ifndef CONFIG_LED_STRIP_GPIO
#define CONFIG_LED_STRIP_GPIO 48
#endif

led_strip_handle_t g_strip = NULL;

static QueueHandle_t alarm_queue = NULL;

static led_mode_t bg_mode = LED_MODE_FIXED;
static led_color_t bg_color = {0, 0, 0};
static int bg_speed = 1000;

void led_task_set_background(led_mode_t mode, led_color_t color, int speed)
{
    bg_mode = mode;
    bg_color = color;
    bg_speed = speed;
}

void led_task_push_alarm(int blinks, led_color_t color)
{
    if (!alarm_queue)
        return;

    alarm_event_t evt = {blinks, color};
    xQueueSend(alarm_queue, &evt, 0);
}

static void led_task(void *arg)
{
    alarm_event_t evt;
    uint32_t step = 0;

    while (1)
    {
        if (xQueueReceive(alarm_queue, &evt, 0) == pdTRUE)
        {
            for (int i = 0; i < evt.blinks; i++)
            {
                led_effect_apply_fixed(evt.color);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_effect_apply_fixed((led_color_t){0, 0, 0});
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
        else
        {
            if (bg_mode == LED_MODE_FIXED)
                led_effect_apply_fixed(bg_color);
            else
                led_effect_apply_breath(bg_color, step++);

            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

esp_err_t led_task_start(void)
{
    ESP_LOGI(TAG, "Initialisation du strip LED...");

    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_LED_STRIP_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
    };

    if (led_strip_new_rmt_device(&strip_config, &rmt_config, &g_strip) != ESP_OK)
        return ESP_FAIL;

    alarm_queue = xQueueCreate(10, sizeof(alarm_event_t));

    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}
