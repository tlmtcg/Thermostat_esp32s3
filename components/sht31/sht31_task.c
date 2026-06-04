/**
 * @file sht31_task.c
 * @brief Tâche FreeRTOS pour la lecture périodique du capteur SHT31 (température/humidité).
 *        Gère les erreurs, les logs sur carte SD, et notifie le thermostat.
 *
 * @note Ce fichier est compilé uniquement si USE_SHT31_SENSOR est défini à 1.
 */

#include "sht31_task.h"
#include <stdio.h>
#include "app_context.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sd_card.h"
#include "time_utils.h"
#include "math.h"


// =========================================================================
// CONFIGURATION
// =========================================================================
/** @brief Active/désactive l'utilisation du capteur SHT31. */
#define USE_SHT31_SENSOR 1

/** @brief Active les logs de débogage pour le SHT31. */
#define SHT31_DEBUG

/** @brief Tag pour les logs ESP. */
static const char *TAG = "SHT31_TASK";

#if USE_SHT31_SENSOR
// Inclusion conditionnelle : seul le code lié au SHT31 est compilé si le capteur est activé
#include "sht31.h"

/** @brief Chemin du fichier de log sur la carte SD. */
#define SHT31_LOG_FILE_PATH MOUNT_POINT "/sht31_data.csv"

/** @brief Nombre d'erreurs consécutives avant tentative de récupération. */
#define SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS 3

/** @brief Intervalle entre les logs sur carte SD (en secondes). */
#define LOG_INTERVAL_MS (5 * 60 * 1000)

/** @brief Timestamp du dernier log SD. */
int64_t last_log_time = 0;
#endif

// =========================================================================
// FONCTION PRINCIPALE DE LA TÂCHE
// =========================================================================

/**
 * @brief Tâche FreeRTOS pour la lecture périodique du SHT31.
 *        - Attend un signal via un EventGroup pour démarrer.
 *        - Lit les données du capteur.
 *        - Gère les erreurs et les récupérations.
 *        - Log les données sur carte SD si activé.
 *
 * @param pvParameters Pointeur vers la configuration de la tâche (sht31_task_config_t).
 */
void sht31_task(void *pvParameters)
{
#if !USE_SHT31_SENSOR
    // --- Si le capteur est désactivé ---
    // Nettoyage des données globales et destruction de la tâche
    g_ctx.temperature = 0.0f;
    g_ctx.humidity = 0.0f;
    ESP_LOGW(TAG, "Capteur SHT31 désactivé via flag -> Auto-destruction de la tâche.");
    vTaskDelete(NULL);
#else
    // --- Initialisation ---
    sht31_task_config_t *task_config = (sht31_task_config_t *)pvParameters;
    bool was_active = false; // Indique si la tâche était active précédemment

// Valeur par défaut pour le délai (en ms)
#define SHT31_DEFAULT_DELAY_MS 5000

    while (1)
    {
        // --- Attente du signal de démarrage ---
        xEventGroupWaitBits(
            task_config->event_group,
            task_config->event_bit,
            pdFALSE, // Ne pas effacer le bit automatiquement
            pdTRUE,  // Attendre que tous les bits soient positionnés
            portMAX_DELAY);

        // --- Mise à jour de l'état "running" ---
        if (!was_active)
        {
            sht31_set_running(true);
            was_active = true;
        }

        // --- Lecture du capteur ---
        float temperature = 0.0f;
        float humidity = 0.0f;
        esp_err_t ret = sht31_read(&temperature, &humidity);

        if (ret == ESP_OK)
        {
            // --- Succès : traitement des données valides ---
            sht31_config_t config;
            char time_str[24];
            char log_buffer[128];

            // Mise à jour des données globales
            g_ctx.temperature = temperature;
            g_ctx.humidity = humidity;

// Log de débogage
#ifdef SHT31_DEBUG
            ESP_LOGD(TAG, "SHT31: %.2f C, %.2f%%", temperature, humidity);
#endif

            // --- Gestion des logs sur carte SD ---
            int64_t now = time_utils_get_timestamp();

            // Vérifier si l'intervalle de log est écoulé
            if ((now - last_log_time) >= (LOG_INTERVAL_MS * 1000))
            {
                last_log_time = now;

                // Vérifier la configuration et l'autorisation d'écriture SD
                sht31_config_t config;
                if (sht31_get_config(&config) == ESP_OK && config.log_to_sd)
                {
                    // Formater les données à écrire
                    char time_str[24];
                    time_utils_get_time_str(time_str, sizeof(time_str));
                    char log_buffer[128];
                    snprintf(
                        log_buffer,
                        sizeof(log_buffer),
                        "%s,%.2f,%.2f\n",
                        time_str,
                        temperature,
                        humidity);

                    // Écrire sur la carte SD avec ton composant
                    if (sd_write_file("/sdcard/sht31_data.csv", log_buffer, "a") != ESP_OK)
                    {
                        ESP_LOGW(TAG, "Échec de l'écriture des logs SHT31 sur la carte SD");
                    }
                }
            }
            else
            {
                // --- Échec : gestion des erreurs ---
                const sht31_runtime_t *runtime = sht31_get_runtime();

                // Vérifier que runtime n'est pas NULL
                if (!runtime)
                {
                    ESP_LOGE(TAG, "sht31_get_runtime() a retourné NULL");
                    g_ctx.temperature = NAN; // Marquer les données comme invalides
                    g_ctx.humidity = NAN;
                    goto manage_delay;
                }

                // Mettre à jour les données globales avec NAN dès la première erreur
                g_ctx.temperature = NAN;
                g_ctx.humidity = NAN;

                // Log des erreurs (filtré pour éviter le spam)
                if (runtime->consecutive_error_count <= SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS ||
                    (runtime->consecutive_error_count % 10) == 0)
                {
                    ESP_LOGW(
                        TAG,
                        "Erreur SHT31: %s (consecutives=%lu)",
                        esp_err_to_name(ret),
                        (unsigned long)runtime->consecutive_error_count);
                }

                // Après un certain nombre d'erreurs, tenter une récupération
                if (runtime->consecutive_error_count >= SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS &&
                    (runtime->consecutive_error_count % SHT31_RECOVER_AFTER_CONSECUTIVE_ERRORS) == 0)
                {
                    esp_err_t recover_ret = sht31_recover();
                    if (recover_ret != ESP_OK)
                    {
                        ESP_LOGW(
                            TAG,
                            "Récupération SHT31 échouée: %s",
                            esp_err_to_name(recover_ret));
                    }
                }
            }

            // --- 4. Logs SD (toutes les 5 minutes) ---
            if (!isnan(g_ctx.temperature))
            {
                int64_t now = time_utils_get_timestamp();
                if ((now - last_log_time) >= LOG_INTERVAL_MS * 1000)
                {
                    last_log_time = now;
                    char time_str[24];
                    char log_buf[128];
                    time_utils_get_time_str(time_str, sizeof(time_str));
                    snprintf(log_buf, sizeof(log_buf), "%s,%.1f,%.1f\n", time_str, g_ctx.temperature, g_ctx.humidity);
                    if (sd_write_file("/sdcard/sht_data.csv", log_buf, "a") != ESP_OK)
                    {
                        ESP_LOGW(TAG, "Échec de l'écriture des logs SHT sur la carte SD");
                    }
                }
            }
            
        // --- Gestion du délai avant la prochaine lecture ---
        manage_delay:
        {
            sht31_config_t config;
            uint32_t delay_ms = SHT31_DEFAULT_DELAY_MS; // Valeur par défaut

            // 1. Vérifier si task_config->delay_ms est valide
            if (task_config->delay_ms != NULL)
            {
                delay_ms = *task_config->delay_ms;
            }

            // 2. Vérifier si la config a un intervalle personnalisé
            if (sht31_get_config(&config) == ESP_OK && config.read_interval_ms > 0)
            {
                delay_ms = config.read_interval_ms;
            }

            // 3. S'assurer que delay_ms n'est pas à 0
            if (delay_ms == 0)
            {
                delay_ms = SHT31_DEFAULT_DELAY_MS;
            }

            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

            // --- Mise à jour de l'état "running" si la tâche est arrêtée ---
            if ((xEventGroupGetBits(task_config->event_group) & task_config->event_bit) == 0)
            {
                sht31_set_running(false);
                was_active = false;
            }
        }
#endif // USE_SHT31_SENSOR
}
}
