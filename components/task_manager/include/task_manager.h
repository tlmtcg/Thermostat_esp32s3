#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

/* --- Bits de contrôle (Event Group) --- */
#define BIT_WEATHER_EN   (1 << 0)
#define BIT_JEEDOM_EN    (1 << 1)
#define BIT_NTP_EN       (1 << 2)

/**
 * @brief Initialise le groupe d'événements et lance les tâches système.
 */
void task_manager_init(void);

/**
 * @brief Active ou désactive une tâche via son bit.
 */
void task_manager_set_active(uint32_t bit, bool active);

/**
 * @brief Génère un objet JSON complet contenant les stats de toutes les tâches.
 * @return cJSON* (doit être supprimé avec cJSON_Delete par l'appelant)
 */
cJSON* task_manager_get_all_info_json(void);

#endif