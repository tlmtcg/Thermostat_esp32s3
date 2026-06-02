#ifndef CRITICAL_H
#define CRITICAL_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Calcule et applique l'état du relais de chauffage en mode dégradé (PWM temporel)
 * * @param ext_temp Température extérieure actuelle
 * @param ext_temp_valide Flag indiquant si le capteur extérieur est fonctionnel
 * @return true si le chauffage doit être activé, false sinon
 */
bool critical_compute_fallback_heating(float ext_temp, bool ext_temp_valide);

#endif // CRITICAL_H