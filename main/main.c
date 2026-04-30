#include <stdio.h>
#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    wifi_callbacks_t cb = {0}; // aucun callback
    wifi_manager_init(&cb);
}
