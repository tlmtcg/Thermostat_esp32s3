#include "sht31.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include <time.h>

static const char *TAG = "SHT31";

void sht31_start(i2c_port_t port, uint8_t addr)
{
    sht31_init(&g_dev, port, addr);

    xTaskCreate(
        sht31_task,
        "sht31_task",
        4096,
        NULL,
        5,
        NULL
    );
}

static void sht31_task(void *arg)
{
    while (1) {
        float t, h;

        if (sht31_read(&g_dev, &t, &h) == ESP_OK) {

            g_state.temperature = t;
            g_state.humidity = h;
            g_state.valid = true;
            g_state.last_update = time(NULL);

        } else {
            g_state.valid = false;
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
