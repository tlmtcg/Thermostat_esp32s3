#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Configuration modifiable du relais.
 */
typedef struct
{
    int gpio_pin;          // GPIO utilise pour piloter le relais
    bool inverted;         // true = actif a 0, false = actif a 1
    uint32_t min_delay_s;  // Delai minimum entre deux changements
} relay_config_t;

/**
 * @brief Etat dynamique du relais.
 *
 * Cette structure est mise a jour en temps reel.
 */
typedef struct
{
    bool state;                // true = ON, false = OFF
    uint32_t cycle_count;      // Nombre total de cycles ON/OFF
    uint32_t total_heating_s;  // Temps total ON en secondes
    char duration_str[32];     // Duree formatee (optionnel)
    char last_error[64];       // Derniere erreur
    uint32_t remaining_s;      // Temps restant avant autorisation ON/OFF
    uint32_t last_change_ms;   // Timestamp en millisecondes
} relay_runtime_t;

/* -------------------------------------------------------------------------- */
/*  API RELAIS                                                                */
/* -------------------------------------------------------------------------- */

void relay_init(void);

void relay_on(void);

void relay_off(void);

void relay_set(bool target_state);

esp_err_t relay_get_config(relay_config_t *out);

esp_err_t relay_set_config(const relay_config_t *config);

const relay_runtime_t *relay_get_runtime(void);

esp_err_t relay_set_gpio(int new_gpio);

esp_err_t relay_set_inverted(bool inverted);

char *relay_get_json_status(void);

esp_err_t relay_apply_config(bool force_state, uint32_t new_min_delay);

bool get_relay_state(void);
