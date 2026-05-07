#include "relay.h"
#include "freertos/FreeRTOS.h" // Nécessaire pour pdMS_TO_TICKS et les types FreeRTOS
#include "freertos/task.h"     // Nécessaire pour vTaskDelay

void test_relay(void) {
    relay_init();

    while(1) {
        relay_on();
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        relay_off(); // Sera refuse si CONFIG_RELAY_MIN_DELAY_SEC > 1
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}