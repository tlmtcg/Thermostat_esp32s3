#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/uart.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "heating_program.h"
#include "time_utils.h"
#include "task_manager.h"
#include "serial_manager.h"
#include "alert_manager.h"
#include "wifi_manager.h"

static const char *TAG = "SERIAL_MGR";

#define BUF_SIZE 256

/* =========================================================
 * COMMANDES CHAUFFAGE
 * ========================================================= */

/**
 * HEAT SET <J> <IDX> <H> <M> <S> <TEMP>
 */
static void do_heat_set(const char *arg)
{
    int j, idx, h, m, s;
    float t;

    // Accès direct à la config globale (SAFE)
    chauffage_config_t *cfg = heating_get_config_rw();

    if (sscanf(arg, "%d %d %d %d %d %f", &j, &idx, &h, &m, &s, &t) == 6)
    {
        heating_set_point( (jour_t)j, idx, h, m, s, t);

        // Sauvegarde NVS
        heating_save();

        printf("\n[OK] Planning mis à jour (Jour %d)\n", j);
    }
    else
    {
        printf("\n[ERR] Usage: HEAT SET <J> <IDX> <H> <M> <S> <T>\n");
    }
}

/**
 * HEAT GET <J> <H> <M> <S>
 */
static void do_heat_get(const char *arg)
{
    int j, h, m, s;

    const chauffage_config_t *cfg = heating_get_config();

    if (sscanf(arg, "%d %d %d %d", &j, &h, &m, &s) == 4)
    {
        uint32_t sec = (h * 3600) + (m * 60) + s;

        float t = heating_get_temp( (jour_t)j, sec);

        printf("\nTemp Jour %d %02d:%02d:%02d => %.1f°C\n", j, h, m, s, t);
    }
    else
    {
        printf("\n[ERR] Usage: HEAT GET <J> <H> <M> <S>\n");
    }
}

/**
 * HEAT JSON
 */
static void do_heat_json(const char *arg)
{
    const chauffage_config_t *cfg = heating_get_config();

    char *json = heating_get_json();

    if (json)
    {
        printf("\n%s\n", json);
        free(json);
    }
    else
    {
        printf("\n[ERR] JSON generation failed\n");
    }
}

/**
 * HEAT RESET
 */
static void do_heat_reset(const char *arg)
{
    chauffage_config_t *cfg = heating_get_config_rw();

    heating_reset_defaults();

    printf("\n[OK] Planning reset defaults\n");
}

/* =========================================================
 * ALERTES / WIFI / SYSTEM
 * ========================================================= */

static void do_alarm_add(const char *arg) { alert_add(arg); }
static void do_alarm_rem(const char *arg) { alert_remove(arg); }

static void do_wifi_fail(const char *arg) { alert_add("Panne wifi"); }
static void do_wifi_ok(const char *arg) { alert_remove("Panne wifi"); }

static void do_wifi_disc(const char *arg) { wifi_manager_force_disconnect(); }
static void do_wifi_conn(const char *arg) { esp_wifi_connect(); }

static void do_status(const char *arg)
{
    printf("\n--- ALERTES ACTIVES: %d ---\n", alert_get_active_count());
    alert_get_history();
}

static void do_time_status_dump(const char *arg)
{
    time_utils_status_dump();
}

/* =========================================================
 * STRUCTURE COMMANDES
 * ========================================================= */

typedef void (*cmd_handler_t)(const char *);

typedef struct
{
    const char *name;
    const char *help;
    cmd_handler_t handler;
} command_t;

static void do_help(const char *arg);

static const command_t cmd_list[] = {
    {"HELP", "Affiche aide", do_help},

    {"STATUS", "Etat systeme", do_status},

    {"ALARM ADD ", "Ajout alarme", do_alarm_add},
    {"ALARM REMOVE ", "Supprimer alarme", do_alarm_rem},

    {"WIFI FAIL", "Simule panne wifi", do_wifi_fail},
    {"WIFI OK", "Simule retour wifi", do_wifi_ok},
    {"WIFI DISCONNECT", "Force deconnexion wifi", do_wifi_disc},
    {"WIFI CONNECT", "Force connexion wifi", do_wifi_conn},

    {"HEAT SET ", "HEAT SET <J> <IDX> <H> <M> <S> <T>", do_heat_set},
    {"HEAT GET ", "HEAT GET <J> <H> <M> <S>", do_heat_get},
    {"HEAT JSON", "Export JSON planning", do_heat_json},
    {"HEAT RESET", "Reset planning", do_heat_reset},

    {"TIME STATUS", "Etat heure systeme", do_time_status_dump},
};

#define CMD_COUNT (sizeof(cmd_list) / sizeof(command_t))

/* =========================================================
 * HELP AUTO
 * ========================================================= */

static void do_help(const char *arg)
{
    printf("\nCOMMANDES DISPONIBLES:\n");

    for (int i = 0; i < CMD_COUNT; i++)
    {
        printf("  - %-20s : %s\n",
               cmd_list[i].name,
               cmd_list[i].help);
    }
}

/* =========================================================
 * DISPATCH COMMANDES
 * ========================================================= */

void handle_command(const char *cmd)
{
    if (!cmd || strlen(cmd) == 0)
        return;

    ESP_LOGD(TAG, "CMD: %s", cmd);

    for (int i = 0; i < CMD_COUNT; i++)
    {
        size_t len = strlen(cmd_list[i].name);

        if (strncmp(cmd, cmd_list[i].name, len) == 0)
        {
            cmd_list[i].handler(cmd + len);
            printf("\n> ");
            fflush(stdout);
            return;
        }
    }

    printf("\nCommande inconnue. Tape HELP\n> ");
    fflush(stdout);
}

/* =========================================================
 * TASK UART
 * ========================================================= */

void serial_task(void *arg)
{
    uint8_t c;
    char line[BUF_SIZE];
    int pos = 0;

    EventGroupHandle_t ev_group = task_manager_get_event_group();

    printf("\nTerminal prêt\n> ");
    fflush(stdout);

    while (1)
    {
        // Bloque si serial désactivé
        xEventGroupWaitBits(ev_group,
                            BIT_SERIAL_EN,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        int len = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(20));

        if (len > 0)
        {
            if (c == '\n' || c == '\r')
            {
                line[pos] = '\0';
                handle_command(line);
                pos = 0;
            }
            else if (c == 0x08 || c == 0x7F)
            {
                if (pos > 0) pos--;
                uart_write_bytes(UART_NUM_0, "\b \b", 3);
            }
            else if (pos < BUF_SIZE - 1 && c >= 32 && c <= 126)
            {
                line[pos++] = c;
                uart_write_bytes(UART_NUM_0, (const char *)&c, 1);
            }
        }
    }
}

/* =========================================================
 * INIT UART
 * ========================================================= */

void serial_manager_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &cfg));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0));
}
