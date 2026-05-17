#pragma once

#include <stdbool.h>

#define HISTORY_SIZE 128

typedef struct
{
    float temperature;
    float humidity;
    float setpoint;

    float temp_history[HISTORY_SIZE];
    float hum_history[HISTORY_SIZE];

    char wifi_ip[16];

    bool wifi_connected;

} app_context_t;

extern app_context_t g_ctx;

void app_context_init(void);
