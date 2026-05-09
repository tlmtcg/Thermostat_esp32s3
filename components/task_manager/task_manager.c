#include "task_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

/* Includes de tes composants */
#include "weather.h"
#include "weather_store.h"
#include "jeedom.h"
#include "esp_sntp.h"
#include "time_utils.h"
#include "led_task.h"
#include "alert_storage.h"
#include "serial_manager.h"

static const char *TAG = "TASK_MGR";
static EventGroupHandle_t s_task_event_group;

/* --- Structure pour le monitoring et la configuration --- */
typedef struct
{
    const char *pcName;     // Nom affiché (FreeRTOS)
    const char *key;        // Identifiant pour l'API Web
    uint32_t usStackDepth;  // Taille de pile allouée
    UBaseType_t uxPriority; // Priorité
    uint32_t event_bit;     // Bit d'activation
    TaskHandle_t pxTask;    // Handle (rempli à la création)
    uint32_t delay_ms;      // Délai de mise à jour
} task_info_t;

/* --- Tableau centralisé des tâches --- */
static task_info_t my_tasks[] = {
    {"Meteo", "weather", 6200, 5, BIT_WEATHER_EN, NULL, 15 * 60 * 1000},
    {"Jeedom", "jeedom", 3100, 5, BIT_JEEDOM_EN, NULL, 60 * 1000},
    {"NTP", "ntp", 2100, 5, BIT_NTP_EN, NULL, 60 * 1000},
    {"Led", "led", 4600, 5, BIT_LED_EN, NULL, 0},
    {"Storage", "storage", 8000, 10, BIT_STORAGE_EN, NULL, 0},
    {"Serial", "serial", 4000, 5, BIT_SERIAL_EN, NULL, 0},
};

#define TASK_COUNT (sizeof(my_tasks) / sizeof(task_info_t))

/* --- Implémentation des tâches avec WaitBits --- */

static void weather_update_task(void *pvParameters)
{
    while (1)
    {
        // Attend le bit d'activation (ne consomme rien si désactivé)
        xEventGroupWaitBits(s_task_event_group, BIT_WEATHER_EN, pdFALSE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "Démarrage du cycle de mise à jour météo...");

        weather_data_t tmp_data;
        // Appel de ta fonction de récupération (ex: OpenMeteo)
        esp_err_t ret = weather_update(&tmp_data);

        if (ret == ESP_OK)
        {
            // Sauvegarde dans ton store local pour l'affichage
            weather_store_set_all(&tmp_data);
            ESP_LOGI(TAG, "Météo mise à jour avec succès.");
        }
        else
        {
            ESP_LOGE(TAG, "Échec de la mise à jour météo (Erreur: %s)", esp_err_to_name(ret));
        }

        // Pause de 15 minutes avant le prochain check
        vTaskDelay(pdMS_TO_TICKS(my_tasks[0].delay_ms));
    }
}

static void jeedom_send_task(void *pvParameters)
{
    while (1)
    {
        // Attend le bit d'activation
        xEventGroupWaitBits(s_task_event_group, BIT_JEEDOM_EN, pdFALSE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "Envoi des données à Jeedom...");

        // Appel de ta fonction globale d'envoi
        // Note : On suppose qu'elle gère ses propres logs internes
        SendStatusJeedom();

        // Pause de 1 minute
        vTaskDelay(pdMS_TO_TICKS(my_tasks[1].delay_ms));
    }
}

static void ntp_monitor_task(void *pvParameters)
{
    while (1)
    {
        // Attend le bit d'activation
        xEventGroupWaitBits(s_task_event_group, BIT_NTP_EN, pdFALSE, pdTRUE, portMAX_DELAY);

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Si l'année est < 2024, l'heure n'est probablement pas à jour
        if (timeinfo.tm_year < (2024 - 1900))
        {
            ESP_LOGW(TAG, "Heure non synchronisée. Tentative NTP...");

            // On s'assure que le service est lancé
            if (!esp_sntp_enabled())
            {
                esp_sntp_init();
            }
        }
        else
        {
            ESP_LOGD(TAG, "Heure OK : %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        }

        // Vérification toutes les minutes
        vTaskDelay(pdMS_TO_TICKS(my_tasks[2].delay_ms));
    }
}

/* --- Fonctions publiques --- */

void task_manager_init(void)
{
    s_task_event_group = xEventGroupCreate();

    // IMPORTANT : On ne met AUCUN bit ici. On attend le WiFi.
    xEventGroupClearBits(s_task_event_group, 0xFFFFFF);
    // Pour voir les logs de la tâche Serial dès le début
    xEventGroupSetBits(s_task_event_group, BIT_SERIAL_EN);

    // Initialisation des composants
    serial_manager_init();
    weather_store_init();
    alert_storage_init("/sdcard/alerts.log");

    // Création des tâches et récupération des handles
    xTaskCreate(weather_update_task, my_tasks[0].pcName, my_tasks[0].usStackDepth, NULL, my_tasks[0].uxPriority, &my_tasks[0].pxTask);
    xTaskCreate(jeedom_send_task, my_tasks[1].pcName, my_tasks[1].usStackDepth, NULL, my_tasks[1].uxPriority, &my_tasks[1].pxTask);
    xTaskCreate(ntp_monitor_task, my_tasks[2].pcName, my_tasks[2].usStackDepth, NULL, my_tasks[2].uxPriority, &my_tasks[2].pxTask);
    xTaskCreate(led_task, my_tasks[3].pcName, my_tasks[3].usStackDepth, NULL, my_tasks[3].uxPriority, &my_tasks[3].pxTask);    
    xTaskCreate(alert_storage_task, my_tasks[4].pcName, my_tasks[4].usStackDepth, NULL, my_tasks[4].uxPriority, &my_tasks[4].pxTask);
    xTaskCreate(serial_task, my_tasks[5].pcName, my_tasks[5].usStackDepth, NULL, my_tasks[5].uxPriority, &my_tasks[5].pxTask);  
}

void task_manager_set_active(uint32_t bit, bool active)
{
    // SÉCURITÉ : Idem ici
    if (s_task_event_group == NULL)
        return;

    if (active)
        xEventGroupSetBits(s_task_event_group, bit);
    else
        xEventGroupClearBits(s_task_event_group, bit);
}

cJSON *task_manager_get_all_info_json(void)
{

    // SÉCURITÉ : Si l'Event Group n'est pas encore prêt, on quitte proprement
    if (s_task_event_group == NULL)
    {
        ESP_LOGW(TAG, "Tentative d'accès au Task Manager avant initialisation");
        return cJSON_CreateArray(); // Retourne un tableau vide au lieu de crash
    }

    cJSON *root = cJSON_CreateArray();
    uint32_t current_bits = xEventGroupGetBits(s_task_event_group);

    for (int i = 0; i < TASK_COUNT; i++)
    {
        cJSON *item = cJSON_CreateObject();
        TaskStatus_t details;

        // Informations de base (statiques)
        cJSON_AddStringToObject(item, "name", my_tasks[i].pcName);
        cJSON_AddStringToObject(item, "key", my_tasks[i].key);
        cJSON_AddNumberToObject(item, "stack_cfg", my_tasks[i].usStackDepth);
        cJSON_AddBoolToObject(item, "active", (current_bits & my_tasks[i].event_bit) != 0);
        cJSON_AddNumberToObject(item, "delay_min", my_tasks[i].delay_ms / 60000);

        // Informations dynamiques (si la tâche existe)
        if (my_tasks[i].pxTask != NULL)
        {
            vTaskGetInfo(my_tasks[i].pxTask, &details, pdTRUE, eInvalid);

            cJSON_AddNumberToObject(item, "prio_curr", details.uxCurrentPriority);
            cJSON_AddNumberToObject(item, "stack_min_ever", details.usStackHighWaterMark);

            const char *st = "Inconnu";
            switch (details.eCurrentState)
            {
            case eRunning:
                st = "Running";
                break;
            case eReady:
                st = "Ready";
                break;
            case eBlocked:
                st = "Blocked";
                break;
            case eSuspended:
                st = "Suspended";
                break;
            default:
                break;
            }
            cJSON_AddStringToObject(item, "state", st);
        }
        cJSON_AddItemToArray(root, item);
    }
    return root;
}

void task_manager_set_delay(const char* key, uint32_t delay_ms) {
    if (delay_ms < 1000) delay_ms = 1000; 
    
    for (int i = 0; i < TASK_COUNT; i++) {
        if (strcmp(my_tasks[i].key, key) == 0) {
            my_tasks[i].delay_ms = delay_ms;
            ESP_LOGI("TASK_MGR", "Nouveau délai pour %s : %lu ms", key, delay_ms);
            break;
        }
    }
}

// Envoi le handler via le get
EventGroupHandle_t task_manager_get_event_group(void) {
    return s_task_event_group;
}
