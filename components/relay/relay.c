#include "relay.h"

#include <string.h>

#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "RELAY";

#ifdef CONFIG_RELAY_INVERTED
#define RELAY_DEFAULT_INVERTED true
#else
#define RELAY_DEFAULT_INVERTED false
#endif

static relay_config_t g_relay_config = {
    .gpio_pin = CONFIG_RELAY_GPIO_PIN,
    .inverted = RELAY_DEFAULT_INVERTED,
    .min_delay_s = CONFIG_RELAY_MIN_DELAY_SEC,
};

static relay_runtime_t g_relay_runtime = {
    .last_error = "",
    .remaining_s = 0,
};

static uint32_t get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void relay_update_remaining_s(void)
{
    uint32_t now = get_ms();
    uint32_t elapsed_s = (now - g_relay_runtime.last_change_ms) / 1000;

    if (elapsed_s < g_relay_config.min_delay_s)
        g_relay_runtime.remaining_s = g_relay_config.min_delay_s - elapsed_s;
    else
        g_relay_runtime.remaining_s = 0;
}

static void relay_configure_gpio(int gpio)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_config(&io_conf);
}

static int relay_level_for_state(bool state)
{
    if (state)
        return g_relay_config.inverted ? 0 : 1;

    return g_relay_config.inverted ? 1 : 0;
}

static bool is_change_allowed(void)
{
    uint32_t now = get_ms();
    uint32_t elapsed_s = (now - g_relay_runtime.last_change_ms) / 1000;

    if (elapsed_s < g_relay_config.min_delay_s)
    {
        g_relay_runtime.remaining_s = g_relay_config.min_delay_s - elapsed_s;

        snprintf(g_relay_runtime.last_error,
                 sizeof(g_relay_runtime.last_error),
                 "Delai trop court : %us restantes",
                 (unsigned)g_relay_runtime.remaining_s);

        ESP_LOGW(TAG, "%s", g_relay_runtime.last_error);
        return false;
    }

    g_relay_runtime.last_error[0] = '\0';
    g_relay_runtime.remaining_s = 0;
    return true;
}

void relay_init(void)
{
    relay_configure_gpio(g_relay_config.gpio_pin);
    gpio_set_level(g_relay_config.gpio_pin, relay_level_for_state(false));

    g_relay_runtime.state = false;
    g_relay_runtime.last_change_ms = get_ms();

    ESP_LOGI(TAG, "Relais init (GPIO %d, min_delay=%us, inverted=%d)",
             g_relay_config.gpio_pin,
             (unsigned)g_relay_config.min_delay_s,
             g_relay_config.inverted);
}

void relay_on(void)
{
    if (!g_relay_runtime.state)
    {
        if (!is_change_allowed())
            return;

        gpio_set_level(g_relay_config.gpio_pin, relay_level_for_state(true));

        g_relay_runtime.state = true;
        g_relay_runtime.cycle_count++;
        g_relay_runtime.last_change_ms = get_ms();

        ESP_LOGI(TAG, "Relais allume");
    }
}

void relay_off(void)
{
    if (g_relay_runtime.state)
    {
        if (!is_change_allowed())
            return;

        uint32_t now = get_ms();
        g_relay_runtime.total_heating_s +=
            (now - g_relay_runtime.last_change_ms) / 1000;

        gpio_set_level(g_relay_config.gpio_pin, relay_level_for_state(false));

        g_relay_runtime.state = false;
        g_relay_runtime.last_change_ms = now;

        ESP_LOGI(TAG, "Relais eteint");
    }
}

void relay_set(bool target_state)
{
    if (target_state)
        relay_on();
    else
        relay_off();
}

esp_err_t relay_get_config(relay_config_t *out)
{
    if (!out)
        return ESP_ERR_INVALID_ARG;

    *out = g_relay_config;
    return ESP_OK;
}

esp_err_t relay_set_config(const relay_config_t *config)
{
    if (!config)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = relay_set_gpio(config->gpio_pin);
    if (err != ESP_OK)
        return err;

    err = relay_set_inverted(config->inverted);
    if (err != ESP_OK)
        return err;

    g_relay_config.min_delay_s = config->min_delay_s;

    ESP_LOGI(TAG, "Config relais appliquee (GPIO %d, min_delay=%us, inverted=%d)",
             g_relay_config.gpio_pin,
             (unsigned)g_relay_config.min_delay_s,
             g_relay_config.inverted);

    return ESP_OK;
}

const relay_runtime_t *relay_get_runtime(void)
{
    relay_update_remaining_s();
    return &g_relay_runtime;
}

esp_err_t relay_set_gpio(int new_gpio)
{
    if (new_gpio < 0 || new_gpio > 48)
        return ESP_ERR_INVALID_ARG;

    g_relay_config.gpio_pin = new_gpio;

    relay_configure_gpio(new_gpio);
    gpio_set_level(new_gpio, relay_level_for_state(g_relay_runtime.state));

    ESP_LOGI(TAG, "GPIO du relais change en %d", new_gpio);
    return ESP_OK;
}

esp_err_t relay_set_inverted(bool inverted)
{
    g_relay_config.inverted = inverted;

    gpio_set_level(g_relay_config.gpio_pin,
                   relay_level_for_state(g_relay_runtime.state));

    ESP_LOGI(TAG, "Inversion modifiee : %s", inverted ? "true" : "false");
    return ESP_OK;
}

char *relay_get_json_status(void)
{
    relay_update_remaining_s();

    cJSON *root = cJSON_CreateObject();

    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddBoolToObject(runtime, "state", g_relay_runtime.state);
    cJSON_AddNumberToObject(runtime, "cycles", g_relay_runtime.cycle_count);
    cJSON_AddNumberToObject(runtime, "total_s", g_relay_runtime.total_heating_s);
    cJSON_AddStringToObject(runtime, "last_error", g_relay_runtime.last_error);
    cJSON_AddNumberToObject(runtime, "remaining_s", g_relay_runtime.remaining_s);

    cJSON *config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddNumberToObject(config, "gpio", g_relay_config.gpio_pin);
    cJSON_AddBoolToObject(config, "inverted", g_relay_config.inverted);
    cJSON_AddNumberToObject(config, "min_delay_s", g_relay_config.min_delay_s);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

esp_err_t relay_apply_config(bool force_state, uint32_t new_min_delay)
{
    if (new_min_delay > 0)
        g_relay_config.min_delay_s = new_min_delay;

    relay_set(force_state);

    return ESP_OK;
}

bool get_relay_state(void)
{
    return g_relay_runtime.state;
}
