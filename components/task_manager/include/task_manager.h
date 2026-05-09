#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

/* 1. Noyau FreeRTOS (Toujours en premier) */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* 2. Bibliothèques standards C */
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

/* --- Bits de contrôle (Event Group) --- */
#define BIT_WEATHER_EN   (1 << 0)
#define BIT_JEEDOM_EN    (1 << 1)
#define BIT_NTP_EN       (1 << 2)
#define BIT_LED_EN       (1 << 3)
#define BIT_STORAGE_EN   (1 << 4)
#define BIT_SERIAL_EN  (1 << 5)

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

/**
 * Met à jour le délai d'attente d'une tâche
 * @param key La clé de la tâche (ex: "weather")
 * @param delay_ms Le nouveau délai en millisecondes
 */
void task_manager_set_delay(const char* key, uint32_t delay_ms);

EventGroupHandle_t task_manager_get_event_group(void);

#endif