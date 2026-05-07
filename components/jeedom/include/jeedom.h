#ifndef JEEDOM_MANAGER_H
#define JEEDOM_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_http_server.h"

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

esp_err_t get_jeedom_config_handler(httpd_req_t *req);
esp_err_t post_jeedom_config_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // JEEDOM_MANAGER_H