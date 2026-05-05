#include "serial_manager.h"
#include "alert_manager.h"
#include "wifi_manager.h"  // Pour wifi_manager_force_disconnect
#include "esp_wifi.h"      // Pour esp_wifi_connect
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"


static const char *TAG = "SERIAL_MGR";
#define BUF_SIZE 256

void handle_command(const char *cmd)
{
    // On ignore les commandes vides (ex: juste un appui sur Entrée)
    if (strlen(cmd) == 0)
        return;

    ESP_LOGI(TAG, "CMD recue: %s", cmd);

    if (strncmp(cmd, "ALARM ADD ", 10) == 0)
    {
        alert_add(cmd + 10);
    }
    else if (strncmp(cmd, "ALARM REMOVE ", 13) == 0)
    {
        alert_remove(cmd + 13);
    }
    else if (strcmp(cmd, "WIFI FAIL") == 0)
    {
        alert_add("Panne wifi");
    }
    else if (strcmp(cmd, "WIFI OK") == 0)
    {
        alert_remove("Panne wifi");
    }
    else if (strcmp(cmd, "STATUS") == 0)
    {
        printf("\n--- Alertes actives: %d ---\n", alert_get_active_count());
        alert_get_history();
    }
    else if (strcmp(cmd, "HELP") == 0)
    {
        printf("\nCommandes: ALARM ADD <nom>, ALARM REMOVE <nom>, WIFI FAIL, WIFI OK, STATUS\n");
    }
    else if (strcmp(cmd, "WIFI DISCONNECT") == 0)
    {
        wifi_manager_force_disconnect();
    }

    else if (strcmp(cmd, "WIFI CONNECT") == 0)
    {
        ESP_LOGI(TAG, "Tentative de reconnexion...");
        esp_wifi_connect();
    }
    else
    {
        printf("\nCommande inconnue [%s]. Tapez HELP.\n", cmd);
    }
    printf("> "); // Réaffiche le prompt après traitement
    fflush(stdout);
}

static void serial_task(void *arg)
{
    uint8_t c;
    char line[BUF_SIZE];
    int line_pos = 0;

    printf("\nTerminal pret. Tapez une commande :\n> ");
    fflush(stdout);

    while (1)
    {
        // Lecture d'un seul octet (bloquant jusqu'au timeout)
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

    // Création de la tâche (priorité légèrement supérieure à la normale)
    xTaskCreate(serial_task, "serial_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Serial manager actif.");
}
