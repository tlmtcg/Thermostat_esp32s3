#include "weather_store.h"
#include <string.h>

static weather_data_t store;
static SemaphoreHandle_t mutex;

void weather_store_init(void)
{
    mutex = xSemaphoreCreateMutex();
    memset(&store, 0, sizeof(store));
}

void weather_store_set_all(const weather_data_t *src)
{
    if (!src) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    store = *src;   // copie complète
    xSemaphoreGive(mutex);
}

void weather_store_get_all(weather_data_t *dst)
{
    if (!dst) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    *dst = store;
    xSemaphoreGive(mutex);
}

const weather_entry_t *weather_store_get_current(void) { return &store.current; }

float weather_store_get_jee_temp(void) { return store.current.jee_temp;}
