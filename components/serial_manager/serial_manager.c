#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"     // Pour esp_wifi_connect
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "driver/uart.h"
#include "heating_program.h"
#include "time_utils.h"
#include "task_manager.h"
#include "serial_manager.h"
#include "alert_manager.h"
#include "wifi_manager.h" // Pour wifi_manager_force_disconnect

static const char *TAG = "SERIAL_MGR";
#define BUF_SIZE 256

// Commande : HEAT SET <JOUR_IDX> <IDX> <H> <M> <S> <TEMP>
// Exemple : HEAT SET 0 0 7 30 0 21.5 (Lundi, Plage 0, 07:30:00, 21.5°C)
static void do_heat_set(const char *arg)
{
    int j, idx, h, m, s;
    float t;

    if (sscanf(arg, "%d %d %d %d %d %f", &j, &idx, &h, &m, &s, &t) == 6)
    {
        heating_set_point(&config, (jour_t)j, idx, h, m, s, t);
        heating_save(&config);
        printf("\n[OK] Planning mis a jour (Jour index %d).\n", j);
    }
    else
    {
        printf("\n[ERR] Usage: HEAT SET <J_IDX> <IDX> <H> <M> <S> <T>\n");
    }
}

// Commande : HEAT GET <JOUR_IDX> <H> <M> <S>
static void do_heat_get(const char *arg)
{
    int j, h, m, s;

    if (sscanf(arg, "%d %d %d %d", &j, &h, &m, &s) == 4)
    {
        uint32_t total_sec = (h * 3600) + (m * 60) + s;
        float t = heating_get_temp(&config, (jour_t)j, total_sec);
        printf("\nConsigne Jour %d a %02d:%02d:%02d : %.1f C\n", j, h, m, s, t);
    }
    else
    {
        printf("\n[ERR] Usage: HEAT GET <J_IDX> <H> <M> <S>\n");
    }
}

static void do_heat_json(const char *arg)
{
    char *json_out = heating_get_json(&config);
    if (json_out)
    {
        printf("\n%s\n", json_out);
        free(json_out); // Très important pour éviter les fuites mémoire
    }
    else
    {
        printf("\n[ERR] Impossible de générer le JSON\n");
    }
}

static void do_heat_reset(const char *arg) { heating_reset_defaults(&config); }

// Prototype de la fonction de callback
typedef void (*cmd_handler_t)(const char *arg);

typedef struct
{
    const char *name;      // Nom de la commande (ex: "STATUS")
    const char *help;      // Description pour l'aide
    cmd_handler_t handler; // Fonction à appeler
} command_t;

// --- Liste des fonctions de traitement ---

static void do_help(const char *arg); // Prototype pour usage dans le tableau

static void do_alarm_add(const char *arg) { alert_add(arg); }
static void do_alarm_rem(const char *arg) { alert_remove(arg); }
static void do_wifi_fail(const char *arg) { alert_add("Panne wifi"); }
static void do_wifi_ok(const char *arg) { alert_remove("Panne wifi"); }
static void do_status(const char *arg)
{
    printf("\n--- Alertes actives: %d ---\n", alert_get_active_count());
    alert_get_history();
}
static void do_wifi_disc(const char *arg) { wifi_manager_force_disconnect(); }
static void do_wifi_conn(const char *arg) { esp_wifi_connect(); }

static void do_time_status_dump(const char *arg) { time_utils_status_dump(); }


// --- Tableau des commandes (à enrichir) ---

static const command_t cmd_list[] = {
    {"HELP", "Affiche cette aide", do_help},
    {"STATUS", "Etat des alertes", do_status},
    {"ALARM ADD ", "Ajoute une alerte <nom>", do_alarm_add},
    {"ALARM REMOVE ", "Supprime une alerte <nom>", do_alarm_rem},
    {"WIFI FAIL", "Simule panne wifi", do_wifi_fail},
    {"WIFI OK", "Simule retour wifi", do_wifi_ok},
    {"WIFI DISCONNECT", "Force deconnexion", do_wifi_disc},
    {"WIFI CONNECT", "Force reconnexion", do_wifi_conn},
    {"HEAT SET ", "Regler: <J_IDX 0-6> <IDX> <H> <M> <S> <T>", do_heat_set},
    {"HEAT GET ", "Lire: <J_IDX 0-6> <H> <M> <S>", do_heat_get},
    {"HEAT GET_JSON", "Lire le programme json", do_heat_json},
    {"HEAT RESET", "Reset le programme json", do_heat_reset},
    {"TIME STATUS", "Affiche le runtime NTP actuelle", do_time_status_dump},
};

#define CMD_COUNT (sizeof(cmd_list) / sizeof(command_t))

// Implémentation du Help automatique
static void do_help(const char *arg)
{
    printf("\nCommandes disponibles :\n");
    for (int i = 0; i < CMD_COUNT; i++)
    {
        printf("  - %-15s : %s\n", cmd_list[i].name, cmd_list[i].help);
    }
}

void handle_command(const char *cmd)
{
    if (strlen(cmd) == 0)
        return;

    ESP_LOGD(TAG, "Traitement: %s", cmd);

    for (int i = 0; i < CMD_COUNT; i++)
    {
        size_t name_len = strlen(cmd_list[i].name);

        // Vérifie si le début de la ligne correspond au nom de la commande
        if (strncmp(cmd, cmd_list[i].name, name_len) == 0)
        {
            // On passe le reste de la chaîne comme argument (ex: le nom de l'alarme)
            cmd_list[i].handler(cmd + name_len);
            printf("\n> ");
            fflush(stdout);
            return;
        }
    }

    printf("\nCommande inconnue. Tapez HELP.\n> ");
    fflush(stdout);
}

void serial_task(void *arg)
{
    uint8_t c;
    char line[BUF_SIZE];
    int line_pos = 0;
    EventGroupHandle_t ev_group = task_manager_get_event_group();

    printf("\nTerminal pret. Tapez une commande :\n> ");
    fflush(stdout);

    while (1)
    {
        // 1. BLOCAGE TOTAL si désactivé par le Web
        // On reste ici indéfiniment tant que le bit est à 0
        xEventGroupWaitBits(ev_group, BIT_SERIAL_EN, pdFALSE, pdTRUE, portMAX_DELAY);
        
        // 2. LECTURE SI ACTIVE
        // Si on est ici, c'est que le Serial est "ON". 
        // On lit les données par petits morceaux.
        int len = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(20));

        if (len > 0)
        {
            // Gestion du retour chariot ou nouvelle ligne
            if (c == '\n' || c == '\r')
            {
                line[line_pos] = '\0';
                handle_command(line);
                line_pos = 0;
            }
            // Gestion simplifiée du Backspace (0x08 ou 0x7F)
            else if (c == 0x08 || c == 0x7F)
            {
                if (line_pos > 0)
                {
                    line_pos--;
                    uart_write_bytes(UART_NUM_0, "\b \b", 3); // Efface visuellement
                }
            }
            // Ajout du caractère si imprimable
            else if (line_pos < BUF_SIZE - 1 && c >= 32 && c <= 126)
            {
                line[line_pos++] = c;
                uart_write_bytes(UART_NUM_0, (const char *)&c, 1); // Echo
            }
        }
    }
}

void serial_manager_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Configuration et installation
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    // Utilisation d'un buffer RX pour ne pas perdre de données
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0));

    // Création de la tâche
    // Note : La création de la tâche est maintenant gérée dans le task_manager pour un meilleur contrôle global
    // xTaskCreate(serial_task, "serial_task", 4096, NULL, 5, NULL);
    // ESP_LOGI(TAG, "Serial manager actif.");
}
