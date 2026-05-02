#pragma once

#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
