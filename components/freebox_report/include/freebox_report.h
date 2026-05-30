#ifndef FREEBOX_REPORT_H
#define FREEBOX_REPORT_H

#include "app_context.h" // Ou le fichier qui contient la définition de app_context_t

/**
 * @brief Tâche FreeRTOS de synchronisation de l'historique vers la Freebox
 */
void freebox_history_sync_task(void *pvParameters);

#endif // FREEBOX_REPORT_H
