#pragma once

#include "alert_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise le stockage SD et enregistre le callback.
 *  path : chemin du fichier log (ex: MOUNT_POINT "/alerts.log")
 *
 *  ⚠️ Doit être appelé APRÈS init_sd_card()
 *     et AVANT alert_manager_init()
 */
void alert_storage_init(const char* path);

/** Recharge l’historique depuis la SD dans alert_manager */
void alert_storage_load(void);

/** Purge automatique si le fichier dépasse la taille limite */
void alert_storage_purge(void);

/** Rotation manuelle : alerts.log → alerts.log.1 */
void alert_storage_rotate(void);

#ifdef __cplusplus
}
#endif

