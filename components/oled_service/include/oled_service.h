#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"

#define TEMP_HISTORY_SIZE 30
#define GRAPH_WIDTH       128
#define GRAPH_HEIGHT      64
#define GRAPH_Y_OFFSET    48
#define GRAPH_X_START     0
#define TEMP_MIN          15.0f
#define TEMP_MAX          35.0f

typedef struct {
    float temp_history[TEMP_HISTORY_SIZE];
    uint8_t history_index;
    bool history_full;
} oled_graph_data_t;

typedef enum
{
    OLED_PAGE_MAIN = 0,
    OLED_PAGE_HISTORY,
    OLED_PAGE_WIFI,
    OLED_PAGE_TIME,
    OLED_PAGE_WEATHER,
    OLED_PAGE_ALERTS,
    OLED_PAGE_COUNT
} oled_page_t;

esp_err_t oled_service_init(i2c_master_bus_handle_t bus);
void oled_service_start(void);
void oled_service_show_boot(void);
void oled_service_show_error(const char *msg);
void oled_service_show_text(const char *line1, const char *line2, const char *line3);
void oled_service_show_temp_hum(float temp, float hum);
void oled_service_add_temp_to_history(float temp);
