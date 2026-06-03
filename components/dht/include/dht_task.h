#ifndef DHT_TASK_H
#define DHT_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Structure de configuration passée en paramètre à la tâche dht_task
typedef struct
{
    EventGroupHandle_t event_group; // Groupe d'événements pour la synchronisation
    EventBits_t event_bit;          // Bit à attendre pour déclencher la lecture
    uint32_t *delay_ms;             // Pointeur vers le délai de boucle (config dynamique)
    uint32_t *read_interval_ms;     // Ajout
    bool *log_to_sd;                // Ajout
} dht_task_config_t;

/**
 * @brief Tâche FreeRTOS principale pour la gestion du capteur DHT (11 ou 22)
 * @param pvParameters Pointeur vers une structure dht_task_config_t
 */
void dht_task(void *pvParameters);

#endif // DHT_TASK_H
