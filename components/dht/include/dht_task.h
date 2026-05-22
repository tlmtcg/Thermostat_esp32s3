#ifndef DHT_TASK_H
#define DHT_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    EventGroupHandle_t event_group;
    EventBits_t event_bit;
    uint32_t *delay_ms;
} dht_task_config_t;

void dht_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // DHT_TASK_H
