#include "oled_service.h"

#include <stdio.h>
#include <string.h>

#include "alert_manager.h"
#include "app_context.h"
#include "esp_log.h"
#include "font3x5.h"
#include "font5x7.h"
#include "relay.h"
#include "time_utils.h"
#include "weather.h"
#include "weather_store.h"

static const char *TAG = "OLED_SERVICE";

#define OLED_I2C_ADDR 0x3C
#define OLED_TASK_STACK_SIZE 4096
#define OLED_TASK_PRIORITY 5
#define OLED_TASK_DELAY_MS 500
#define OLED_LINE_BUFFER_SIZE 32
#define OLED_PAGE_DURATION_MS 5000
#define OLED_HISTORY_SAMPLE_MS 10000
#define OLED_HEADER_HEIGHT 16
#define OLED_MAIN_SEPARATOR_Y 15
#define OLED_GRAPH_TOP_Y 19
#define OLED_GRAPH_BOTTOM_Y 63
#define OLED_GRAPH_LEFT_X 16
#define OLED_GRAPH_RIGHT_X 127

static oled_graph_data_t graph_data = {0};
static oled_page_t current_page = OLED_PAGE_MAIN;
static TickType_t last_switch = 0;
static TickType_t last_history_sample = 0;
static TaskHandle_t display_task_handle = NULL;

static bool oled_is_ready(void);
static void oled_draw_error(const char *msg);
static void oled_task(void *arg);
static void draw_centered_string(uint8_t y, const char *str);
static void history_add_temperature(float temp);
static void maybe_sample_history(void);
static void draw_main_page(void);
static void draw_history_page(void);
static void draw_wifi_page(void);
static void draw_time_page(void);
static void draw_weather_page(void);
static void draw_alert_page(void);
static void draw_hline(uint8_t y, uint8_t x1, uint8_t x2);
static void draw_vline(uint8_t x, uint8_t y1, uint8_t y2);
static void draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
static void draw_status_dot(uint8_t x, uint8_t y, bool enabled);
static uint8_t clamp_u8_int(int value, int min_value, int max_value);
static size_t get_history_point_count(void);
static void history_stats(float *out_min, float *out_max, float *out_latest);
static void draw_temp_graph_area(uint8_t origin_y);
static const char *weather_code_short_description(int code);

static bool oled_is_ready(void)
{
    return oled.initialized;
}

static void oled_draw_error(const char *msg)
{
    if (!oled_is_ready())
    {
        return;
    }

    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, 0, "ERROR");
    if (msg)
    {
        ssd1306_draw_string(&oled, 0, 16, msg);
    }
    ssd1306_update(&oled);
}

static void draw_centered_string(uint8_t y, const char *str)
{
    if (!oled_is_ready() || str == NULL)
    {
        return;
    }

    int len = (int)strlen(str);
    int x = (SSD1306_WIDTH - (len * 6)) / 2;
    if (x < 0)
    {
        x = 0;
    }

    ssd1306_draw_string(&oled, (uint8_t)x, y, str);
}

void history_add_sample(float temp, float hum)
{
    memmove(&g_ctx.temp_history[0], &g_ctx.temp_history[1],
            (HISTORY_SIZE - 1) * sizeof(float));

    memmove(&g_ctx.hum_history[0], &g_ctx.hum_history[1],
            (HISTORY_SIZE - 1) * sizeof(float));

    memmove(&g_ctx.ts_history[0], &g_ctx.ts_history[1],
            (HISTORY_SIZE - 1) * sizeof(uint64_t));

    g_ctx.temp_history[HISTORY_SIZE - 1] = temp;
    g_ctx.hum_history[HISTORY_SIZE - 1] = hum;
    g_ctx.ts_history[HISTORY_SIZE - 1] = time_utils_get_timestamp();
}

static void history_add_temperature(float temp)
{
    memmove(
        &g_ctx.temp_history[0],
        &g_ctx.temp_history[1],
        (HISTORY_SIZE - 1) * sizeof(float));

    g_ctx.temp_history[HISTORY_SIZE - 1] = temp;
}

static void history_add_humidity(float hum)
{
    memmove(
        &g_ctx.hum_history[0],
        &g_ctx.hum_history[1],
        (HISTORY_SIZE - 1) * sizeof(float));

    g_ctx.hum_history[HISTORY_SIZE - 1] = hum;
}

static void maybe_sample_history(void)
{
    TickType_t now = xTaskGetTickCount();

    if (last_history_sample == 0 ||
        (now - last_history_sample) >= pdMS_TO_TICKS(OLED_HISTORY_SAMPLE_MS))
    {
        history_add_temperature(g_ctx.temperature);
        history_add_humidity(g_ctx.humidity);

        last_history_sample = now;
    }
}

void oled_service_add_temp_to_history(float temp)
{
    graph_data.temp_history[graph_data.history_index] = temp;
    graph_data.history_index = (graph_data.history_index + 1) % TEMP_HISTORY_SIZE;
    if (graph_data.history_index == 0)
    {
        graph_data.history_full = true;
    }
}

static void draw_main_page(void)
{
    char line[OLED_LINE_BUFFER_SIZE];
    char time_buffer[16];
    bool relay_on = get_relay_state();

    time_utils_get_hour_str(time_buffer, sizeof(time_buffer));

    draw_centered_string(4, time_buffer);
    draw_box(0, 0, SSD1306_WIDTH, OLED_HEADER_HEIGHT);

    draw_box(0, 18, 76, 18);
    ssd1306_draw_string(&oled, 4, 21, "TEMP");
    snprintf(line, sizeof(line), "%.1f C", g_ctx.temperature);
    ssd1306_draw_string(&oled, 28, 21, line);

    draw_box(80, 18, 48, 18);
    ssd1306_draw_string(&oled, 84, 21, "SET");
    snprintf(line, sizeof(line), "%.1f", g_ctx.setpoint);
    ssd1306_draw_string(&oled, 102, 21, line);

    draw_box(0, 40, 62, 12);
    ssd1306_draw_string(&oled, 4, 42, "HUM");
    snprintf(line, sizeof(line), "%.0f%%", g_ctx.humidity);
    ssd1306_draw_string(&oled, 30, 42, line);

    draw_box(66, 40, 62, 12);
    draw_status_dot(72, 44, g_ctx.wifi_connected);
    ssd1306_draw_string(&oled, 79, 42, "WF");
    ssd1306_draw_string(&oled, 91, 42, g_ctx.wifi_connected ? "ON" : "OFF");

    draw_box(0, 54, 128, 10);
    draw_status_dot(4, 57, relay_on);
    ssd1306_draw_string(&oled, 12, 56, "RELAIS");
    ssd1306_draw_string(&oled, 48, 56, relay_on ? "ON" : "OFF");
    snprintf(line, sizeof(line), "IP %.*s", 11, g_ctx.wifi_connected ? g_ctx.wifi_ip : "-");
    ssd1306_draw_string(&oled, 72, 56, line);
}

static void draw_wifi_page(void)
{
    char line[OLED_LINE_BUFFER_SIZE];
    draw_centered_string(4, "Etat WiFi");
    draw_box(0, 0, SSD1306_WIDTH, OLED_HEADER_HEIGHT);

    if (!g_ctx.wifi_connected)
    {
        draw_box(4, 22, 120, 18);
        draw_centered_string(28, "Deconnecte");
        return;
    }

    draw_box(4, 22, 120, 18);
    draw_centered_string(28, "Connecte");
    draw_box(4, 44, 120, 16);
    snprintf(line, sizeof(line), "IP %.*s", 15, g_ctx.wifi_ip);
    ssd1306_draw_string(&oled, 10, 49, line);
}

static void draw_history_page(void)
{
    char line[OLED_LINE_BUFFER_SIZE];
    float min_temp = 0.0f;
    float max_temp = 0.0f;
    float latest_temp = 0.0f;

    history_stats(&min_temp, &max_temp, &latest_temp);

    draw_centered_string(4, "Courbe Temp");
    draw_box(0, 0, SSD1306_WIDTH, OLED_HEADER_HEIGHT);
    snprintf(line, sizeof(line), "Now %.1fC", latest_temp);
    ssd1306_draw_string(&oled, 0, 18, line);
    snprintf(line, sizeof(line), "Lo %.1f Hi %.1f", min_temp, max_temp);
    ssd1306_draw_string(&oled, 54, 18, line);
    draw_temp_graph_area(27);
}

static void draw_time_page(void)
{
    char time_buffer[OLED_LINE_BUFFER_SIZE];
    time_utils_get_time_str(time_buffer, sizeof(time_buffer));
    draw_centered_string(4, "Horloge");
    draw_box(0, 0, SSD1306_WIDTH, OLED_HEADER_HEIGHT);
    draw_box(4, 24, 120, 20);
    draw_centered_string(31, time_buffer);
}

static void draw_weather_page(void)
{
    weather_data_t weather = {0};
    char line[OLED_LINE_BUFFER_SIZE];
    const char *desc;
    float delta_48h;

    weather_store_get_all(&weather);
    desc = weather_code_short_description(weather.current.weather_code);
    delta_48h = weather.forecast_48h.temperature - weather.current.temperature;

    draw_centered_string(4, "Meteo");
    draw_box(0, 0, SSD1306_WIDTH, OLED_HEADER_HEIGHT);

    draw_box(0, 18, 128, 14);
    draw_centered_string(22, desc);

    draw_box(0, 35, 62, 14);
    ssd1306_draw_string(&oled, 4, 39, "EXT");
    snprintf(line, sizeof(line), "%.1fC", weather.current.temperature);
    ssd1306_draw_string(&oled, 28, 39, line);

    draw_box(66, 35, 62, 14);
    ssd1306_draw_string(&oled, 70, 39, "HUM");
    snprintf(line, sizeof(line), "%.0f%%", weather.current.humidity);
    ssd1306_draw_string(&oled, 94, 39, line);

    draw_box(0, 52, 128, 12);
    snprintf(
        line,
        sizeof(line),
        "48h %.1fC %s%.1f",
        weather.forecast_48h.temperature,
        delta_48h >= 0.0f ? "+" : "",
        delta_48h);
    ssd1306_draw_string(&oled, 4, 55, line);
}

static void draw_alert_page(void)
{
    alert_log_t active_alerts[CONFIG_MAX_ACTIVE_ALERTS];
    int count = alert_get_active_alerts(active_alerts, CONFIG_MAX_ACTIVE_ALERTS);
    int y = 18;
    int shown = 0;

    draw_centered_string(4, "Alertes");
    draw_box(0, 0, SSD1306_WIDTH, OLED_HEADER_HEIGHT);

    for (int i = 0; i < count && shown < 4; i++)
    {
        if (active_alerts[i].name[0] == '\0')
        {
            continue;
        }

        char display_name[22] = {0};
        snprintf(
            display_name,
            sizeof(display_name),
            "%.*s",
            (int)sizeof(display_name) - 1,
            active_alerts[i].name);

        draw_box(2, (uint8_t)y, 124, 10);
        ssd1306_draw_string(&oled, 6, (uint8_t)(y + 2), display_name);
        y += 12;
        shown++;
    }

    if (shown == 0)
    {
        draw_box(4, 26, 120, 16);
        draw_centered_string(31, "Aucune alarme");
    }
}

static void draw_hline(uint8_t y, uint8_t x1, uint8_t x2)
{
    ssd1306_draw_line(&oled, x1, y, x2, y, 1);
}

static void draw_vline(uint8_t x, uint8_t y1, uint8_t y2)
{
    ssd1306_draw_line(&oled, x, y1, x, y2, 1);
}

static void draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    if (w < 2 || h < 2)
    {
        return;
    }

    draw_hline(y, x, x + w - 1);
    draw_hline(y + h - 1, x, x + w - 1);
    draw_vline(x, y, y + h - 1);
    draw_vline(x + w - 1, y, y + h - 1);
}

static void draw_status_dot(uint8_t x, uint8_t y, bool enabled)
{
    for (int dy = 0; dy < 4; dy++)
    {
        for (int dx = 0; dx < 4; dx++)
        {
            ssd1306_draw_pixel(&oled, x + dx, y + dy, enabled);
        }
    }

    if (!enabled)
    {
        draw_box(x, y, 4, 4);
    }
}

static uint8_t clamp_u8_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return (uint8_t)min_value;
    }
    if (value > max_value)
    {
        return (uint8_t)max_value;
    }
    return (uint8_t)value;
}

static const char *weather_code_short_description(int code)
{
    switch (code)
    {
    case 0:
        return "Clair";
    case 1:
        return "Peu nuageux";
    case 2:
        return "Nuages";
    case 3:
        return "Couvert";
    case 45:
    case 48:
        return "Brouillard";
    case 51:
    case 53:
    case 55:
        return "Bruine";
    case 61:
    case 63:
    case 65:
        return "Pluie";
    case 71:
    case 73:
    case 75:
        return "Neige";
    case 80:
    case 81:
    case 82:
        return "Averses";
    case 95:
    case 96:
    case 99:
        return "Orage";
    default:
        return "Inconnu";
    }
}

static size_t get_history_point_count(void)
{
    size_t count = 0;

    for (size_t i = 0; i < HISTORY_SIZE; i++)
    {
        if (g_ctx.temp_history[i] > 0.0f)
        {
            count++;
        }
    }

    return count;
}

static void history_stats(float *out_min, float *out_max, float *out_latest)
{
    float min_temp = g_ctx.temperature;
    float max_temp = g_ctx.temperature;
    float latest_temp = g_ctx.temperature;
    bool found = false;

    for (size_t i = 0; i < HISTORY_SIZE; i++)
    {
        float value = g_ctx.temp_history[i];
        if (value <= 0.0f)
        {
            continue;
        }

        if (!found)
        {
            min_temp = value;
            max_temp = value;
            found = true;
        }

        if (value < min_temp)
        {
            min_temp = value;
        }
        if (value > max_temp)
        {
            max_temp = value;
        }

        latest_temp = value;
    }

    if (out_min)
    {
        *out_min = min_temp;
    }
    if (out_max)
    {
        *out_max = max_temp;
    }
    if (out_latest)
    {
        *out_latest = latest_temp;
    }
}

static void draw_temp_graph_area(uint8_t origin_y)
{
    char label[8];
    float raw_min = 0.0f;
    float raw_max = 0.0f;
    float latest = 0.0f;
    size_t point_count = get_history_point_count();

    if (point_count < 2)
    {
        draw_centered_string(36, "Pas assez de donnees");
        return;
    }

    history_stats(&raw_min, &raw_max, &latest);

    float scale_min = (float)((int)(raw_min - 1.0f));
    float scale_max = (float)((int)(raw_max + 2.0f));
    if ((scale_max - scale_min) < 4.0f)
    {
        float center = (raw_min + raw_max) * 0.5f;
        scale_min = center - 2.0f;
        scale_max = center + 2.0f;
    }

    int graph_top = origin_y;
    int graph_bottom = OLED_GRAPH_BOTTOM_Y;
    int graph_height = graph_bottom - graph_top;
    int graph_left = OLED_GRAPH_LEFT_X;
    int graph_right = OLED_GRAPH_RIGHT_X;
    int graph_width = graph_right - graph_left;
    float scale_mid = (scale_min + scale_max) * 0.5f;

    snprintf(label, sizeof(label), "%.0f", scale_max);
    ssd1306_draw_string(&oled, 0, (uint8_t)(graph_top - 2), label);
    snprintf(label, sizeof(label), "%.0f", scale_mid);
    ssd1306_draw_string(&oled, 0, (uint8_t)(graph_top + graph_height / 2 - 3), label);
    snprintf(label, sizeof(label), "%.0f", scale_min);
    ssd1306_draw_string(&oled, 0, (uint8_t)(graph_bottom - 7), label);

    draw_box((uint8_t)graph_left, (uint8_t)graph_top, (uint8_t)(graph_width + 1), (uint8_t)(graph_height + 1));
    draw_hline((uint8_t)(graph_top + graph_height / 2), (uint8_t)graph_left, (uint8_t)graph_right);

    for (int x = graph_left + 16; x < graph_right; x += 16)
    {
        draw_vline((uint8_t)x, (uint8_t)graph_top, (uint8_t)graph_bottom);
    }

    int prev_x = -1;
    int prev_y = -1;
    size_t first_index = HISTORY_SIZE - point_count;

    for (size_t i = 0; i < point_count; i++)
    {
        float value = g_ctx.temp_history[first_index + i];
        float normalized = (value - scale_min) / (scale_max - scale_min);
        int x = graph_left + (int)((i * graph_width) / (point_count - 1));
        int y = graph_bottom - (int)(normalized * graph_height);
        y = clamp_u8_int(y, graph_top, graph_bottom);

        if (prev_x >= 0)
        {
            ssd1306_draw_line(&oled, (uint8_t)prev_x, (uint8_t)prev_y, (uint8_t)x, (uint8_t)y, 1);
        }

        prev_x = x;
        prev_y = y;
    }

    draw_box(
        clamp_u8_int(prev_x - 1, graph_left, graph_right - 2),
        clamp_u8_int(prev_y - 1, graph_top, graph_bottom - 2),
        3,
        3);
}

static void oled_task(void *arg)
{
    (void)arg;

    while (1)
    {
        if (!oled_is_ready())
        {
            vTaskDelay(pdMS_TO_TICKS(OLED_TASK_DELAY_MS));
            continue;
        }

        maybe_sample_history();

        TickType_t now = xTaskGetTickCount();

        if ((now - last_switch) > pdMS_TO_TICKS(OLED_PAGE_DURATION_MS))
        {
            current_page = (current_page + 1) % OLED_PAGE_COUNT;
            last_switch = now;
        }

        ssd1306_clear(&oled);

        switch (current_page)
        {
        case OLED_PAGE_MAIN:
            draw_main_page();
            break;
        case OLED_PAGE_HISTORY:
            draw_history_page();
            break;
        case OLED_PAGE_WIFI:
            draw_wifi_page();
            break;
        case OLED_PAGE_TIME:
            draw_time_page();
            break;
        case OLED_PAGE_WEATHER:
            draw_weather_page();
            break;
        case OLED_PAGE_ALERTS:
            draw_alert_page();
            break;
        default:
            break;
        }

        if (ssd1306_update(&oled) != ESP_OK)
        {
            ESP_LOGE(TAG, "OLED update failed");
        }

        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_DELAY_MS));
    }
}

esp_err_t oled_service_init(i2c_master_bus_handle_t bus)
{
    configASSERT(bus != NULL && "I2C bus handle cannot be NULL");

    ESP_LOGI(TAG, "Initializing OLED...");

    esp_err_t err = ssd1306_init(&oled, bus, OLED_I2C_ADDR);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ssd1306_init failed: %s", esp_err_to_name(err));
        return err;
    }

    graph_data.history_index = 0;
    graph_data.history_full = false;
    for (uint8_t i = 0; i < TEMP_HISTORY_SIZE; i++)
    {
        graph_data.temp_history[i] = TEMP_MIN;
    }

    oled_service_show_boot();
    return ESP_OK;
}

void oled_service_start(void)
{
    if (!oled_is_ready())
    {
        ESP_LOGW(TAG, "OLED not initialized, skipping task start");
        return;
    }

    if (display_task_handle != NULL)
    {
        ESP_LOGW(TAG, "OLED task already started");
        return;
    }

    last_switch = xTaskGetTickCount();

    BaseType_t res = xTaskCreate(
        oled_task,
        "oled_task",
        OLED_TASK_STACK_SIZE,
        NULL,
        OLED_TASK_PRIORITY,
        &display_task_handle);

    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create OLED task");
    }
}

void oled_service_show_boot(void)
{
    if (!oled_is_ready())
    {
        return;
    }

    ssd1306_clear(&oled);
    draw_centered_string(8, "THERMOSTAT");
    draw_centered_string(24, "Starting...");
    ssd1306_update(&oled);
}

void oled_service_show_error(const char *msg)
{
    oled_draw_error(msg);
}

void oled_service_show_text(const char *line1, const char *line2, const char *line3)
{
    if (!oled_is_ready())
    {
        return;
    }

    ssd1306_clear(&oled);

    if (line1)
    {
        ssd1306_draw_string(&oled, 0, 0, line1);
    }
    if (line2)
    {
        ssd1306_draw_string(&oled, 0, 16, line2);
    }
    if (line3)
    {
        ssd1306_draw_string(&oled, 0, 32, line3);
    }

    ssd1306_update(&oled);
}

void oled_service_show_temp_hum(float temp, float hum)
{
    char line1[OLED_LINE_BUFFER_SIZE];
    char line2[OLED_LINE_BUFFER_SIZE];

    if (!oled_is_ready())
    {
        return;
    }

    snprintf(line1, sizeof(line1), "Temp: %.1f C", temp);
    snprintf(line2, sizeof(line2), "Hum: %.1f %%", hum);

    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, 0, "THERMOSTAT");
    ssd1306_draw_string(&oled, 0, 16, line1);
    ssd1306_draw_string(&oled, 0, 32, line2);

    oled_service_add_temp_to_history(temp);

    ssd1306_update(&oled);
}
