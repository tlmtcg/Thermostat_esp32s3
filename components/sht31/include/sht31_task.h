#ifndef SHT31_TASK_H
#define SHT31_TASK_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

typedef struct
{
    EventGroupHandle_t event_group;
    uint32_t event_bit;
    uint32_t *delay_ms;
} sht31_task_config_t;

void sht31_task(void *pvParameters);

#endif
