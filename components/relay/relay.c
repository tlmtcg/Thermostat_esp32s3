#include "relay.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "RELAY";

/*
 * Structure runtime globale
 * Elle contient TOUT l’état dynamique du relais.
 */
relay_runtime_t g_relay_runtime = {
    .gpio_pin     = CONFIG_RELAY_GPIO_PIN,
    .inverted     = CONFIG_RELAY_INVERTED,
    .min_delay_s  = CONFIG_RELAY_MIN_DELAY_SEC,
    .last_error   = "",
    .remaining_s  = 0
};

/* -------------------------------------------------------------------------- */
/*  UTILITAIRES                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Retourne le temps en millisecondes depuis boot
 */
static uint32_t get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Configure un GPIO en sortie
 */
static void relay_configure_gpio(int gpio)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);
}

/* -------------------------------------------------------------------------- */
/*  SECURITE : DELAI MIN ENTRE 2 COMMUTATIONS                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Vérifie si un changement d’état est autorisé
 *        (sécurité anti-cycles courts)
 */
static bool is_change_allowed(void)
{
    uint32_t now = get_ms();
    uint32_t elapsed_s = (now - g_relay_runtime.last_change_ms) / 1000;

    if (elapsed_s < g_relay_runtime.min_delay_s)
    {
        g_relay_runtime.remaining_s = g_relay_runtime.min_delay_s - elapsed_s;

        snprintf(g_relay_runtime.last_error,
                 sizeof(g_relay_runtime.last_error),
                 "Délai trop court : %lus restantes",
                 g_relay_runtime.remaining_s);

        ESP_LOGW(TAG, "%s", g_relay_runtime.last_error);
        return false;
    }

    // Pas d’erreur
    g_relay_runtime.last_error[0] = '\0';
    g_relay_runtime.remaining_s = 0;
    return true;
}

/* -------------------------------------------------------------------------- */
/*  INITIALISATION                                                            */
/* -------------------------------------------------------------------------- */

void relay_init(void)
{
    relay_configure_gpio(g_relay_runtime.gpio_pin);

    // Niveau logique initial selon inversion
    int level = g_relay_runtime.inverted ? 1 : 0;
    gpio_set_level(g_relay_runtime.gpio_pin, level);

    g_relay_runtime.state = false;
    g_relay_runtime.last_change_ms = get_ms();

    ESP_LOGI(TAG, "Relais init (GPIO %d, min_delay=%ds, inverted=%d)",
             g_relay_runtime.gpio_pin,
             g_relay_runtime.min_delay_s,
             g_relay_runtime.inverted);
}

/* -------------------------------------------------------------------------- */
/*  COMMANDES RELAIS                                                          */
/* -------------------------------------------------------------------------- */

void relay_on(void)
{
    if (!g_relay_runtime.state)
    {
        if (!is_change_allowed())
            return;

        gpio_set_level(
            g_relay_runtime.gpio_pin,
            g_relay_runtime.inverted ? 0 : 1
        );

        g_relay_runtime.state = true;
        g_relay_runtime.cycle_count++;
        g_relay_runtime.last_change_ms = get_ms();

        ESP_LOGI(TAG, "⚡ Relais ALLUME");
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

        gpio_set_level(
            g_relay_runtime.gpio_pin,
            g_relay_runtime.inverted ? 1 : 0
        );

        g_relay_runtime.state = false;
        g_relay_runtime.last_change_ms = now;

        ESP_LOGI(TAG, "🌑 Relais ETEINT");
    }
}

void relay_set(bool target_state)
{
    if (target_state)
        relay_on();
    else
        relay_off();
}

/* -------------------------------------------------------------------------- */
/*  CONFIG RUNTIME : GPIO / INVERSION / DELAI                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Change dynamiquement le GPIO du relais
 */
esp_err_t relay_set_gpio(int new_gpio)
{
    if (new_gpio < 0 || new_gpio > 48)
        return ESP_ERR_INVALID_ARG;

    g_relay_runtime.gpio_pin = new_gpio;

    relay_configure_gpio(new_gpio);

    // Réappliquer l’état actuel
    gpio_set_level(
        new_gpio,
        g_relay_runtime.state
            ? (g_relay_runtime.inverted ? 0 : 1)
            : (g_relay_runtime.inverted ? 1 : 0)
    );

    ESP_LOGI(TAG, "GPIO du relais changé en %d", new_gpio);
    return ESP_OK;
}

/**
 * @brief Change dynamiquement l’inversion du relais
 */
esp_err_t relay_set_inverted(bool inverted)
{
    g_relay_runtime.inverted = inverted;

    gpio_set_level(
        g_relay_runtime.gpio_pin,
        g_relay_runtime.state
            ? (inverted ? 0 : 1)
            : (inverted ? 1 : 0)
    );

    ESP_LOGI(TAG, "Inversion modifiée : %s", inverted ? "true" : "false");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  JSON STATUS                                                               */
/* -------------------------------------------------------------------------- */

char *relay_get_json_status(void)
{
    /* Mise à jour dynamique du temps restant */
    uint32_t now = get_ms();
    uint32_t elapsed_s = (now - g_relay_runtime.last_change_ms) / 1000;

    if (elapsed_s < g_relay_runtime.min_delay_s)
        g_relay_runtime.remaining_s = g_relay_runtime.min_delay_s - elapsed_s;
    else
        g_relay_runtime.remaining_s = 0;

    /* Construction JSON */
    cJSON *root = cJSON_CreateObject();

    // Runtime
    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddBoolToObject(runtime, "state", g_relay_runtime.state);
    cJSON_AddNumberToObject(runtime, "cycles", g_relay_runtime.cycle_count);
    cJSON_AddNumberToObject(runtime, "total_s", g_relay_runtime.total_heating_s);
    cJSON_AddStringToObject(runtime, "duration", g_relay_runtime.duration_str);
    cJSON_AddStringToObject(runtime, "last_error", g_relay_runtime.last_error);
    cJSON_AddNumberToObject(runtime, "remaining_s", g_relay_runtime.remaining_s);

    // Config
    cJSON *config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddNumberToObject(config, "gpio", g_relay_runtime.gpio_pin);
    cJSON_AddBoolToObject(config, "inverted", g_relay_runtime.inverted);
    cJSON_AddNumberToObject(config, "min_delay_s", g_relay_runtime.min_delay_s);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

/* -------------------------------------------------------------------------- */
/*  CONFIG API SIMPLIFIEE                                                     */
/* -------------------------------------------------------------------------- */

esp_err_t relay_apply_config(bool force_state, uint32_t new_min_delay)
{
    if (new_min_delay > 0)
        g_relay_runtime.min_delay_s = new_min_delay;

    relay_set(force_state);

    return ESP_OK;
}

