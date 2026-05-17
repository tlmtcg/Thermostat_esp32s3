#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "display_pages.h"
#include "app_context.h"

#include "ssd1306.h"

#include "esp_log.h"
#include "time_utils.h"
#include "alert_manager.h"

static ssd1306_t *g_lcd = NULL;

static ui_page_t current_page = PAGE_MAIN;

static TickType_t last_switch = 0;

#define PAGE_DURATION_MS 5000
#define SSID_MAX_DISPLAY 20

static const char *TAG = "DISPLAY";

static void draw_main_page(void);
static void draw_history_page(void);
static void draw_wifi_page(void);
static void draw_time_page(void);
static void draw_alert_page(void);

static TaskHandle_t display_task_handle = NULL;

void history_add_temperature(float temp)
{
    memmove(
        &g_ctx.temp_history[0],
        &g_ctx.temp_history[1],
        (HISTORY_SIZE - 1) * sizeof(float));

    g_ctx.temp_history[HISTORY_SIZE - 1] = temp;
}

// Fonction pour centrer une chaîne sur l'écran SSD1306 (128 pixels de large)
static void draw_centered_string(uint8_t y, const char *str) {
    if (!g_lcd || !str) return;

    int len = strlen(str);
    int x = (128 - (len * 6)) / 2; // 6 pixels par caractère (police 8x6)
    if (x < 0) x = 0; // Éviter les valeurs négatives

    ssd1306_draw_string(g_lcd, x, y, str);
}


void display_task(void *arg)
{
    while (1)
    {
        if (g_lcd == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        TickType_t now = xTaskGetTickCount();

        if ((now - last_switch) > pdMS_TO_TICKS(PAGE_DURATION_MS))
        {
            current_page = (current_page + 1) % PAGE_COUNT;
            last_switch = now;
        }

        ssd1306_clear(g_lcd);

        switch (current_page)
        {
        case PAGE_MAIN:
            draw_main_page();
            break;

        case PAGE_HISTORY:
            draw_history_page();
            break;

        case PAGE_WIFI:
            draw_wifi_page();
            break;

        case PAGE_TIME:
            draw_time_page();
            break;

        case PAGE_ALERTS:
            draw_alert_page();
            break;

        default:
            break;
        }

        ssd1306_update(g_lcd);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void display_pages_start(void)
{
    xTaskCreate(
        display_task,   // fonction task
        "display_task", // nom
        4096,           // stack size
        NULL,           // paramètre
        5,              // priorité
        &display_task_handle);
}

static void draw_main_page(void)
{
    char line[32];

    history_add_temperature(g_ctx.temperature);

    snprintf(line, sizeof(line),
             "Temp: %.1f C",
             g_ctx.temperature);

    ssd1306_draw_string(g_lcd, 0, 0, line);

    snprintf(line, sizeof(line),
             "Hum : %.1f %%",
             g_ctx.humidity);

    ssd1306_draw_string(g_lcd, 0, 16, line);

    snprintf(line, sizeof(line),
             "Set : %.1f C",
             g_ctx.setpoint);

    ssd1306_draw_string(g_lcd, 0, 32, line);
}

static void draw_wifi_page(void)
{
    char line[32];

    if (!g_ctx.wifi_connected)
    {
        ssd1306_draw_string(g_lcd, 0, 0, "WiFi disconnected");
        return;
    }

    snprintf(line, sizeof(line),
             "IP: %.*s",
             15,
             g_ctx.wifi_ip);

     draw_centered_string(32, line);
}

static void draw_history_page(void)
{
    const float t_min = 0.0f;
    const float t_max = 40.0f;

    // ===== AXE Y + LABELS (colonne 0) =====
    ssd1306_draw_string(g_lcd, 0, 0, "40");

    ssd1306_draw_string(g_lcd, 0, 28, "20");

    ssd1306_draw_string(g_lcd, 0, 56, "0");

    // petite ligne verticale axe Y
    for (int y = 0; y < 64; y++)
    {
        ssd1306_draw_pixel(g_lcd, 10, y, true);
    }

    // ===== GRAPH =====
    for (int x = 11; x < 127; x++)
    {
        float t = g_ctx.temp_history[x];

        // clamp
        if (t < t_min)
            t = t_min;
        if (t > t_max)
            t = t_max;

        int y = 63 - (int)((t - t_min) * 63.0f / (t_max - t_min));

        if (y < 0)
            y = 0;
        if (y > 63)
            y = 63;

        ssd1306_draw_pixel(g_lcd, x, y, true);
    }
}

static void draw_time_page(void)
{

    char dest[32];
    char line[32];
    time_utils_get_time_str(dest, sizeof(dest));

    snprintf(line, sizeof(dest), "%s", dest);

    draw_centered_string(32, line);
}

void display_pages_init(ssd1306_t *lcd)
{
    g_lcd = lcd;
    ESP_LOGI(TAG, "Display Pages initialisée");
}

static void draw_alert_page(void) {
    if (!g_lcd) {
        ESP_LOGE(TAG, "g_lcd non initialisé !");
        return;
    }

    // --- 1. Effacer l'écran ---
    // ssd1306_clear_screen(g_lcd, false);

    // --- 2. Titre "Liste des alarmes" centré sur la ligne 0 ---
    draw_centered_string(0, "Liste des alarmes");

    // --- 3. Récupérer les alarmes actives ---
    alert_log_t active_alerts[CONFIG_MAX_ACTIVE_ALERTS];
    int count = alert_get_active_alerts(active_alerts, CONFIG_MAX_ACTIVE_ALERTS);

    // Afficher les alarmes à partir de la ligne 3 (y=32)
    int y = 32;
    int shown = 0;

    for (int i = 0; i < count && shown < 4; i++) {
        if (active_alerts[i].name[0] == '\0') continue; // Ignorer les noms vides

        // Tronquer le nom à 21 caractères (pour 128 pixels / 6 = ~21)
        char display_name[22] = {0}; // 21 caractères + '\0'
        snprintf(display_name, sizeof(display_name), "%.*s",
                 (int)sizeof(display_name) - 1, active_alerts[i].name);

        ssd1306_draw_string(g_lcd, 0, y, display_name);
        ESP_LOGI(TAG, "Alarme %d: %s", shown + 1, display_name);

        y += 16;
        shown++;
    }

    // Si aucune alarme, afficher "Aucune alarme" centré
    if (shown == 0) {
        draw_centered_string(32, "Aucune alarme");
    }
}
 