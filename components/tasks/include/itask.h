#pragma once
#include <stdint.h>

typedef struct {
    const char *name;
    const char *key;
    uint32_t default_delay;
    uint32_t event_bit;

    void (*on_start)(void);
    void (*on_run)(void);
    void (*on_stop)(void);
} ITask;

