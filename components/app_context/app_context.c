#include "app_context.h"
#include <string.h>

app_context_t g_ctx = {
    .temperature = 0.0f,
    .humidity = 0.0f,
    .setpoint = 21.0f,
    .wifi_connected = false
};

void app_context_init(void)
{
    // valeurs par défaut capteurs
    g_ctx.temperature = 0.0f;
    g_ctx.humidity = 0.0f;
    g_ctx.setpoint = 20.0f;

    // WiFi default
    g_ctx.wifi_connected = false;
    memset(g_ctx.wifi_ip, 0, sizeof(g_ctx.wifi_ip));

    // historique température
    memset(g_ctx.temp_history, 0, sizeof(g_ctx.temp_history));
}

