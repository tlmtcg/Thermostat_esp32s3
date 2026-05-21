#ifndef TASKS_H
#define TASKS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#define BIT_WEATHER_EN   (1 << 0)
#define BIT_JEEDOM_EN    (1 << 1)
#define BIT_NTP_EN       (1 << 2)
#define BIT_LED_EN       (1 << 3)
#define BIT_STORAGE_EN   (1 << 4)
#define BIT_SERIAL_EN    (1 << 5)
#define BIT_SHT31_EN     (1 << 6)
#define BIT_THERMO_EN    (1 << 7)

typedef struct
{
    const char *pcName;
    const char *key;
    uint32_t usStackDepth;
    UBaseType_t uxPriority;
    uint32_t event_bit;
    TaskHandle_t pxTask;
    uint32_t delay_ms;
} task_info_t;

extern task_info_t my_tasks[];
extern const int TASK_COUNT;

void tasks_init(void);
void tasks_set_active(uint32_t bit, bool active);
cJSON *tasks_get_all_info_json(void);
void tasks_set_delay(const char *key, uint32_t delay_ms);
EventGroupHandle_t tasks_get_event_group(void);

#endif
