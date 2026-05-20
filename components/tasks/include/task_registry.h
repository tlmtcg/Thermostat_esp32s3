#ifndef TASK_REGISTRY_H
#define TASK_REGISTRY_H

#include "tasks.h"

typedef struct
{
    task_info_t *info;
    TaskFunction_t entry;
    void *parameter;
} task_registry_entry_t;

extern task_registry_entry_t task_registry[];
extern const int TASK_REGISTRY_COUNT;

void ntp_monitor_task(void *pvParameters);
void led_task(void *pvParameters);
void alert_storage_task(void *pvParameters);
void serial_task(void *pvParameters);

void task_registry_set_event_group(EventGroupHandle_t event_group);

#endif
