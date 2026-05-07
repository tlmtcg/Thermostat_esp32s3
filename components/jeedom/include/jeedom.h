#ifndef JEEDOM_MANAGER_H
#define JEEDOM_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

// --- Inclusions des dépendances du projet ---
// Ajustez les chemins selon votre structure de dossiers réelle
#include "Constants/Constant.h"
#include "Version.h"
#include "JSON/Model/GlobalModel.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Envoie les données du thermostat à Jeedom.
 * 
 * Cette fonction prépare un objet JSON contenant les états du capteur,
 * de la consigne, du relais et du système, puis l'envoie via une 
 * requête HTTP POST.
 * 
 * @```

### Notes sur ce header :
*   **return true  Si l'envoi a réussi (HTTP 200 ou 201).
 * @return false Si une erreur réseau, HTTP ou une conditionGardes d'inclusion :** Empêchent les erreurs de redéfinition si le fichier est inclus plusieurs fois.
*   **Compatibilité C++ ( de garde échoue.
 */
bool SendStatusJeedom();

#ifdef __cplusplus
}
#endif

#endif // JEEDOM_MANAGER_H