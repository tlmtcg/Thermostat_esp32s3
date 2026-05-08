#ifndef TIME_UTILS_STORAGE_H
#define TIME_UTILS_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char ntp_server[64];
    uint8_t ntp_max_retry;
    uint32_t ntp_sync_interval_sec; // Nom unifié ici
} time_utils_config_t;

// void time_utils_get_cfg(time_utils_config_t *dest);

// Prototypes propres
bool time_utils_storage_load(time_utils_config_t *cfg);
bool time_utils_storage_save(const time_utils_config_t *cfg);
bool time_utils_storage_reset_defaults(void);

#endif
