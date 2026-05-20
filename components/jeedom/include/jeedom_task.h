#ifndef JEEDOM_TASK_H
#define JEEDOM_TASK_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

typedef struct
{
    EventGroupHandle_t event_group;
    uint32_t event_bit;
    uint32_t *delay_ms;
    bool (*is_wifi_connected)(void);
} jeedom_task_config_t;

void jeedom_send_task(void *pvParameters);

#endif
