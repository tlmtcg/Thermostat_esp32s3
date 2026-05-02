#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <stdbool.h>
#include "led_db.h"

#define MAX_ACTIVE_ALERTS 5

typedef struct {
    int alarm_indices[MAX_ACTIVE_ALERTS];
    int count;
} alert_stack_t;

void alert_manager_init(void);
bool alert_add(const char *name);
bool alert_remove(const char *name);
int  alert_get_count(void);

#endif
