#pragma once

#include "alert_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise le backend LED et s’abonne aux callbacks */
void led_backend_init(void);

#ifdef __cplusplus
}
#endif
