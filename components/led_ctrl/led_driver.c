// components/led/led_driver.c
#include "led_driver.h"
#include "led_strip.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "LED_DRIVER";
static led_strip_handle_t strip = NULL;

esp_err_t led_driver_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_LED_STRIP_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec de l'initialisation du driver LED: %s", esp_err_to_name(err));
        return err;
    }

    led_driver_clear();
    return ESP_OK;
}

void led_driver_set_pixel(led_color_t color) {
    if (strip) {
        led_strip_set_pixel(strip, 0, color.r, color.g, color.b);
    }
}

void led_driver_clear(void) {
    if (strip) {
        led_strip_clear(strip);
    }
}

void led_driver_refresh(void) {
    if (strip) {
        led_strip_refresh(strip);
    }
}

void led_driver_set_brightness(uint8_t brightness) {
    // À implémenter si nécessaire (pour APA102)
}