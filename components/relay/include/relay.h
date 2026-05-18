#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Structure contenant tout l'état dynamique du relais.
 *
 * Cette structure est mise à jour en temps réel et renvoyée dans le JSON.
 */
typedef struct
{
    /* État logique du relais */
    bool state;                 // true = ON, false = OFF

    /* Statistiques */
    uint32_t cycle_count;       // Nombre total de cycles ON/OFF
    uint32_t total_heating_s;   // Temps total ON en secondes
    char duration_str[32];      // Durée formatée (optionnel)

    /* Configuration dynamique */
    int gpio_pin;               // GPIO utilisé pour piloter le relais
    bool inverted;              // true = actif à 0, false = actif à 1
    uint32_t min_delay_s;       // Délai minimum entre deux changements

    /* Sécurité / erreurs */
    char last_error[64];        // Dernière erreur (ex: délai trop court)
    uint32_t remaining_s;       // Temps restant avant autorisation ON/OFF

    /* Horodatage du dernier changement */
    uint32_t last_change_ms;    // Timestamp en millisecondes
} relay_runtime_t;

/**
 * @brief Instance globale du runtime
 */
extern relay_runtime_t g_relay_runtime;

/* -------------------------------------------------------------------------- */
/*  API RELAIS                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialise le relais (GPIO, inversion, état initial)
 */
void relay_init(void);

/**
 * @brief Allume le relais (si délai OK)
 */
void relay_on(void);

/**
 * @brief Éteint le relais (si délai OK)
 */
void relay_off(void);

/**
 * @brief Définit l'état du relais (ON/OFF)
 */
void relay_set(bool target_state);

/**
 * @brief Change dynamiquement le GPIO du relais
 */
esp_err_t relay_set_gpio(int new_gpio);

/**
 * @brief Change dynamiquement l'inversion du relais
 */
esp_err_t relay_set_inverted(bool inverted);

/**
 * @brief Renvoie un JSON complet de l'état du relais
 *        (l'appelant doit free() la chaîne)
 */
char *relay_get_json_status(void);

/**
 * @brief Applique une configuration simple (état + min_delay)
 *        Utilisé pour compatibilité avec l'ancien code.
 */
esp_err_t relay_apply_config(bool force_state, uint32_t new_min_delay);

bool get_relay_state();