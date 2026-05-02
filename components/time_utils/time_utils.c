#include "time_utils.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "alert_manager.h"

static const char *TAG = "TIME_UTILS";
static time_t s_last_sync = 0;

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
    ESP_LOGI(TAG, "Initialisation du SNTP");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Fuseau horaire France
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // On ajoute l'alerte dès le début de l'attente
    alert_add("NTP Wait");

    // Attendre la synchro (max 10 essais)
    bool synced = false;
    for (int retry = 0; retry < 10; retry++)
    {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            synced = true;
            break;
        }

        ESP_LOGI(TAG, "Attente de synchro SNTP (%d/10)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (synced) {
        ESP_LOGI(TAG, "Synchro réussie !");
        // On retire l'alerte d'attente
        alert_remove("NTP Wait");
    } else {
        ESP_LOGE(TAG, "Échec de synchro SNTP après 10 essais.");
        // Optionnel : on remplace l'attente par une alerte d'erreur permanente
        alert_remove("NTP Wait");
        alert_add("Time Error"); 
    }

    return synced ? ESP_OK : ESP_FAIL;
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

void time_utils_get_time_str(char *dest, size_t max_size)
{
    struct tm info = time_utils_get_local_time();
    strftime(dest, max_size, "%d/%m/%Y %H:%M:%S", &info);
}

time_t time_utils_get_last_sync(void)
{
    return s_last_sync;
}
