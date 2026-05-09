#pragma once

#include "esp_err.h"

esp_err_t led_task_start(void);
void led_task(void *pvParameters);