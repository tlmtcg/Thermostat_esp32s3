#include "ws_api_logs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_SIZE 4096
static char log_buffer[LOG_BUFFER_SIZE];
static int log_index = 0;
static bool buffer_full = false; // Permet de savoir si le buffer a fait un tour complet

// Mutex pour éviter la concurrence entre l'écriture (Log) et la lecture (HTTP)
static SemaphoreHandle_t log_mutex = NULL; 

static const char *TAG = "WS_LOGS";

int web_log_vprintf(const char *fmt, va_list args)
{
    char tmp[256];
    // Écriture sécurisée dans un tampon temporaire
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);

    if (len > 0 && log_mutex != NULL)
    {
        // On attend le mutex pour écrire dans le buffer circulaire
        if (xSemaphoreTake(log_mutex, portMAX_DELAY) == pdTRUE)
        {
            for (int i = 0; i < len; i++)
            {
                log_buffer[log_index] = tmp[i];
                log_index++;
                if (log_index >= LOG_BUFFER_SIZE)
                {
                    log_index = 0;
                    buffer_full = true; // Le buffer est plein, on écrase les anciens logs
                }
            }
            xSemaphoreGive(log_mutex);
        }
    }

    // Conserve l'affichage classique sur le port série (UART)
    return vprintf(fmt, args);
}

static esp_err_t logs_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");

    if (log_mutex == NULL) {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    xSemaphoreTake(log_mutex, portMAX_DELAY);

    // Scénario 1 : Le buffer a débordé, on doit envoyer la fin du buffer PUIS le début
    if (buffer_full)
    {
        // Partie 1 : Du curseur actuel jusqu'à la fin du tableau (les logs les plus anciens)
        int part1_len = LOG_BUFFER_SIZE - log_index;
        if (part1_len > 0) {
            httpd_resp_send_chunk(req, &log_buffer[log_index], part1_len);
        }
        
        // Partie 2 : Du début du tableau jusqu'au curseur actuel (les logs les plus récents)
        if (log_index > 0) {
            httpd_resp_send_chunk(req, log_buffer, log_index);
        }
    }
    // Scénario 2 : Le buffer n'est pas encore plein, on envoie juste du début jusqu'au curseur
    else
    {
        if (log_index > 0) {
            httpd_resp_send_chunk(req, log_buffer, log_index);
        }
    }

    xSemaphoreGive(log_mutex);

    // Finalisation de la réponse HTTP (Chunk vide obligatoire)
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t ws_register_logs_api(httpd_handle_t server)
{
    // Création du Mutex avant d'activer les logs
    if (log_mutex == NULL) {
        log_mutex = xSemaphoreCreateMutex();
    }

    // Redirection des logs vers notre fonction personnalisée
    esp_log_set_vprintf(web_log_vprintf);

    httpd_uri_t uri = {
        .uri = "/api/logs",
        .method = HTTP_GET,
        .handler = logs_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET logs)", uri.uri);
    esp_err_t err = httpd_register_uri_handler(server, &uri);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Logs API registration FAILED");
        return err;
    }

    ESP_LOGI(TAG, "API Logs enregistrée avec succès");
    return ESP_OK;
}

void init_web_log_capture(void)
{
    if (log_mutex == NULL) {
        log_mutex = xSemaphoreCreateMutex();
    }
    esp_log_set_vprintf(web_log_vprintf);
}
