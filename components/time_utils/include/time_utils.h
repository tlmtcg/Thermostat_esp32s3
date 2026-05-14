#pragma once

#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int current_retry;      // L'essai en cours
    bool is_syncing;        // Si une synchro est active
    uint32_t last_sync_time;// Timestamp de la dernière réussite
} time_status_t;

// Rend l'état accessible
void time_utils_get_status(time_status_t *dest);

/**
 * @brief Initialise SNTP + fuseau horaire + callback.
 */
esp_err_t time_utils_init(void);

/**
 * @brief Retourne l'heure locale sous forme struct tm.
 */
struct tm time_utils_get_local_time(void);

/**
 * @brief Retourne une chaîne formatée JJ/MM/AAAA HH:MM:SS.
 */
void time_utils_get_time_str(char *dest, size_t max_size);

/**
 * @brief Retourne le timestamp de la dernière synchro SNTP.
 */
time_t time_utils_get_last_sync(void);

void time_utils_status_dump();

void time_utils_get_hour_str(char *dest, size_t max);

#ifdef __cplusplus
}
#endif
