#include "time_utils.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "alert_manager.h"
#include "sdkconfig.h"
#include "time_utils_storage.h"

static const char *TAG = "TIME_UTILS";
static time_t s_last_sync = 0;

time_utils_config_t cfg;
// Variable statique privée au fichier
static time_status_t s_time_status = {0};

// Fonction pour que les autres composants lisent l'état
void time_utils_get_status(time_status_t *dest)
{
    if (dest)
        memcpy(dest, &s_time_status, sizeof(time_status_t));
}

/* -------------------------------------------------------------------------- */
/*  CALLBACK SNTP                                                             */
/* -------------------------------------------------------------------------- */

static void time_sync_notification_cb(struct timeval *tv)
{
    s_last_sync = tv->tv_sec;
    ESP_LOGI(TAG, "Synchronisation SNTP réussie");
}

/* -------------------------------------------------------------------------- */
/*  INITIALISATION                                                            */
/* -------------------------------------------------------------------------- */

esp_err_t time_utils_init(void)
{

    // 1. Charger la config (Une seule fois ici, au début)
    if (!time_utils_storage_load(&cfg))
    {
        ESP_LOGW(TAG, "Config NVS introuvable, usage des valeurs par défaut");
        strlcpy(cfg.ntp_server, CONFIG_SNTP_SERVER_NAME, sizeof(cfg.ntp_server));
        cfg.ntp_max_retry = CONFIG_SNTP_MAX_RETRY;
    }

    // 2. Initialisation du service SNTP
    ESP_LOGI(TAG, "Démarrage SNTP : %s (max %d tentatives)", cfg.ntp_server, cfg.ntp_max_retry);

    if (esp_sntp_enabled())
    {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, cfg.ntp_server);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // 3. Configuration Zone Horaire
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // 4. Boucle d'attente de synchronisation
    s_time_status.is_syncing = true;
    alert_add("Attente NTP");
    bool synced = false;

    for (int retry = 0; retry < cfg.ntp_max_retry; retry++)
    {
        // MISE À JOUR RUNTIME
        s_time_status.current_retry = retry + 1;
        if (s_last_sync != 0)
        {
            s_time_status.last_sync_time = (uint32_t)time(NULL);
            s_time_status.is_syncing = false;
            synced = true;
            break;
        }

        // On log l'attente uniquement toutes les 2 secondes pour ne pas saturer la console
        ESP_LOGD(TAG, "Attente synchro NTP... (%d/%d)", retry + 1, cfg.ntp_max_retry);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    s_time_status.is_syncing = false;

    // 5. Nettoyage et résultat
    alert_remove("Attente NTP");

    if (synced)
    {
        ESP_LOGI(TAG, "Heure synchronisée avec succès.");
        alert_remove("Panne NTP");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Le serveur NTP n'a pas répondu. Vérifiez la connexion WiFi.");
        alert_add("Panne NTP");
        return ESP_FAIL;
    }
}

/* -------------------------------------------------------------------------- */
/*  UTILITAIRES TEMPS                                                         */
/* -------------------------------------------------------------------------- */

struct tm time_utils_get_local_time(void)
{
    time_t now;
    struct tm info;
    time(&now);
    localtime_r(&now, &info);
    return info;
}

uint64_t time_utils_get_timestamp(void)
{
    return (uint64_t)time(NULL);
}

void time_utils_get_time_str(char *dest, size_t max_size)
{
    struct tm info = time_utils_get_local_time();
    strftime(dest, max_size, "%d/%m/%Y %H:%M:%S", &info);
}

time_t time_utils_get_last_sync(void)
{
    return s_last_sync;
}

void time_utils_status_dump()
{
    ESP_LOGI(TAG, "=== SNTP STATUS DUMP ===");
    ESP_LOGI(TAG, "NTP Server: %s", cfg.ntp_server);
    ESP_LOGI(TAG, "Max Retry: %d", cfg.ntp_max_retry);
    ESP_LOGI(TAG, "Sync Interval (sec): %d", cfg.ntp_sync_interval_sec);
    ESP_LOGI(TAG, "Current Retry: %d", s_time_status.current_retry);
    ESP_LOGI(TAG, "Is Syncing: %s", s_time_status.is_syncing ? "Yes" : "No");
    if (s_time_status.last_sync_time != 0)
    {
        char time_str[64];
        time_utils_get_time_str(time_str, sizeof(time_str));
        ESP_LOGI(TAG, "Last Sync Time: %s", time_str);
    }
    else
    {
        ESP_LOGI(TAG, "Last Sync Time: Never");
    }
}

void time_utils_get_hour_str(char *dest, size_t max)
{
    char full[32];
    time_utils_get_time_str(full, sizeof(full));
    strncpy(dest, full + 11, max);
}
