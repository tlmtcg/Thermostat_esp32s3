#include "tasks.h"
#include "task_registry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include <string.h>
#include <time.h>

// En-têtes des modules de l'application
#include "weather_store.h"
#include "esp_sntp.h"
#include "alert_storage.h"
#include "serial_manager.h"
#include "wifi_manager.h"
#include "config_storage.h"
#include "thermostat.h"
#include "prediction_engine.h"

// Inclusion obligatoire pour accéder à la variable d'instance "dht_task_config"
#include "dht_task.h" 

static const char *TAG = "TASKS";

// 2. Déclaration EXTERN pour indiquer au compilateur que l'instance réelle est définie ailleurs
extern dht_task_config_t dht_task_config;

// Groupe d'événements FreeRTOS centralisant l'état d'activation des tâches
static EventGroupHandle_t s_task_event_group;

/**
 * @brief Applique la configuration initiale issue du Kconfig (menuconfig ESP-IDF).
 *        Force également l'exécution des tâches matérielles et critiques système.
 */
static void tasks_apply_kconfig(void)
{
    // Remise à zéro complète des bits d'événements
    xEventGroupClearBits(s_task_event_group, 0xFFFFFF);

    // Chargement des commutateurs optionnels définis par Kconfig
    if (CONFIG_BIT_WEATHER_EN)
        xEventGroupSetBits(s_task_event_group, BIT_WEATHER_EN);
    if (CONFIG_BIT_JEEDOM_EN)
        xEventGroupSetBits(s_task_event_group, BIT_JEEDOM_EN);
    if (CONFIG_BIT_NTP_EN)
        xEventGroupSetBits(s_task_event_group, BIT_NTP_EN);
    if (CONFIG_BIT_LED_EN)
        xEventGroupSetBits(s_task_event_group, BIT_LED_EN);
    if (CONFIG_BIT_SERIAL_EN)
        xEventGroupSetBits(s_task_event_group, BIT_SERIAL_EN);

    // SÉCURITÉ MATÉRIELLE : On force l'activation des tâches indispensables
    // pour garantir leur fonctionnement, même en mode dégradé (ex: panne SD)
    xEventGroupSetBits(s_task_event_group, BIT_STORAGE_EN);
    xEventGroupSetBits(s_task_event_group, BIT_SHT31_EN);
    xEventGroupSetBits(s_task_event_group, BIT_THERMO_EN);
    xEventGroupSetBits(s_task_event_group, BIT_DHT_EN); // <-- Activation forcée du DHT
}

/**
 * @brief Analyse et applique la configuration utilisateur lue depuis le fichier JSON.
 * @param root Pointeur vers l'objet racine cJSON.
 */
static void tasks_apply_json(cJSON *root)
{
    if (!root)
        return;

    cJSON *tasks = cJSON_GetObjectItem(root, "tasks");
    if (!cJSON_IsArray(tasks))
        return;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, tasks)
    {
        cJSON *id = cJSON_GetObjectItem(item, "id");
        if (!cJSON_IsString(id))
            continue;

        cJSON *enabled = cJSON_GetObjectItem(item, "enabled");
        cJSON *delay = cJSON_GetObjectItem(item, "delay_ms");

        // Recherche de la correspondance dans le tableau global my_tasks
        for (int i = 0; i < TASK_COUNT; i++)
        {
            if (strcmp(my_tasks[i].key, id->valuestring) == 0)
            {
                // Mise à jour du délai d'exécution de la tâche si fourni
                if (cJSON_IsNumber(delay))
                    my_tasks[i].delay_ms = delay->valueint;

                // Mise à jour de l'état d'activation
                if (cJSON_IsBool(enabled))
                {
                    if (enabled->valueint)
                        xEventGroupSetBits(s_task_event_group, my_tasks[i].event_bit);
                    else
                        xEventGroupClearBits(s_task_event_group, my_tasks[i].event_bit);
                }
            }
        }
    }

    // Sécurités matérielles persistantes pour empêcher qu'un JSON mal configuré 
    // ou corrompu ne coupe les tâches d'acquisition fondamentales.
    xEventGroupSetBits(s_task_event_group, BIT_STORAGE_EN);
    xEventGroupSetBits(s_task_event_group, BIT_DHT_EN); // <-- Maintien forcé post-JSON
}

/**
 * @brief Tâche FreeRTOS de supervision de la synchronisation de l'heure réseau via NTP.
 */
void ntp_monitor_task(void *pvParameters)
{
    while (1)
    {
        // Attente bloquante : la tâche ne s'exécute que si son bit NTP est actif
        xEventGroupWaitBits(s_task_event_group, BIT_NTP_EN, pdFALSE, pdTRUE, portMAX_DELAY);

        if (wifi_get_state() != WIFI_STATE_STA_CONNECTED)
        {
            ESP_LOGW(TAG, "NTP: WiFi non connecte, attente...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Si l'année système est antérieure à 2024, l'heure n'est pas encore synchronisée
        if (timeinfo.tm_year < (2024 - 1900))
        {
            ESP_LOGW(TAG, "Heure non synchronisee. Tentative NTP...");

            if (!esp_sntp_enabled())
            {
                esp_sntp_init();
            }
        }
        else
        {
            ESP_LOGD(TAG, "Heure OK : %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        }

        vTaskDelay(pdMS_TO_TICKS(my_tasks[2].delay_ms)); // Utilise le délai de la table
    }
}

/**
 * @brief Tâche FreeRTOS assurant la régulation thermique (Thermostat).
 */
void thermostat_task(void *pvParameters)
{
    while (1)
    {
        // Remarque : Le bit THERMO_EN n'est volontairement pas bloquant ici
        thermostat_update_current_consigne();
        prediction_engine_tick();
        must_heat();

        vTaskDelay(pdMS_TO_TICKS(my_tasks[7].delay_ms));
    }
}

/**
 * @brief Affiche un rapport complet de la configuration de chaque tâche dans les logs.
 */
static void tasks_dump_config(void)
{
    ESP_LOGI(TAG, "===== CONFIGURATION DES TACHES =====");

    uint32_t bits = xEventGroupGetBits(s_task_event_group);

    for (int i = 0; i < TASK_COUNT; i++)
    {
        bool active = (bits & my_tasks[i].event_bit) != 0;

        ESP_LOGI(TAG,
                 "[%s] key=%s | bit=0x%lX | active=%s | delay=%lu ms | stack=%lu | prio=%lu",
                 my_tasks[i].pcName,
                 my_tasks[i].key,
                 (unsigned long)my_tasks[i].event_bit,
                 active ? "ON" : "OFF",
                 (unsigned long)my_tasks[i].delay_ms,
                 (unsigned long)my_tasks[i].usStackDepth,
                 (unsigned long)my_tasks[i].uxPriority);
    }

    ESP_LOGI(TAG, "====================================");
}

/**
 * @brief Fonction d'initialisation principale du gestionnaire de tâches.
 */
void tasks_init(void)
{
    // 1. Instanciation du groupe d'événements FreeRTOS
    s_task_event_group = xEventGroupCreate();

    // 2. Initialisation des composants et gestionnaires sous-jacents
    serial_manager_init();
    weather_store_init();
    alert_storage_init("/sdcard/alerts.log");

    // 3. CONTOURNEMENT TECHNIQUE : Injection du groupe d'événements dans le DHT.
    // Puisque task_registry_set_event_group() est figée et ne traite pas le DHT, 
    // nous assignons le pointeur directement avant l'appel global.
    dht_task_config.event_group = s_task_event_group; 

    // 4. Application des configurations (Kconfig puis JSON sur carte SD)
    tasks_apply_kconfig();
    task_registry_set_event_group(s_task_event_group); // Configure Weather, Jeedom et SHT31

    cJSON *json = load_json_from_sdcard("/sdcard/tasks.json");
    if (json)
    {
        ESP_LOGI(TAG, "Configuration JSON trouvee sur SD. Application des parametres...");
        tasks_apply_json(json);
        cJSON_Delete(json);
    }

    // 5. Affichage de la topologie finale dans la console de debug
    tasks_dump_config();

    // 6. Boucle de création itérative de l'ensemble des tâches du registre
    for (int i = 0; i < TASK_REGISTRY_COUNT; i++)
    {
        task_registry_entry_t *task = &task_registry[i];
        void *parameter = task->parameter;

        // Exception historique pour le gestionnaire série qui prend le groupe directement en paramètre
        if (task->info->event_bit == BIT_SERIAL_EN)
            parameter = s_task_event_group;

        xTaskCreate(
            task->entry,
            task->info->pcName,
            task->info->usStackDepth,
            parameter,
            task->info->uxPriority,
            &task->info->pxTask);
    }
}

/**
 * @brief Modifie dynamiquement l'état actif/inactif d'une tâche via son bit d'événement.
 */
void tasks_set_active(uint32_t bit, bool active)
{
    if (s_task_event_group == NULL)
        return;

    if (active)
        xEventGroupSetBits(s_task_event_group, bit);
    else
        xEventGroupClearBits(s_task_event_group, bit);

    // Sauvegarde immédiate du nouvel état sur la carte SD
    save_json_to_sdcard("/sdcard/tasks.json");
}

/**
 * @brief Génère la structure JSON contenant la télémétrie d'exécution FreeRTOS.
 * @return Pointeur vers un tableau cJSON (Array). Décochage de la mémoire à la charge de l'appelant.
 */
cJSON *tasks_get_all_info_json(void)
{
    if (s_task_event_group == NULL)
    {
        ESP_LOGW(TAG, "Tentative d'acces aux taches avant initialisation");
        return cJSON_CreateArray();
    }

    cJSON *root = cJSON_CreateArray();
    uint32_t current_bits = xEventGroupGetBits(s_task_event_group);

    for (int i = 0; i < TASK_COUNT; i++)
    {
        cJSON *item = cJSON_CreateObject();
        TaskStatus_t details;

        // Propriétés statiques de configuration
        cJSON_AddStringToObject(item, "name", my_tasks[i].pcName);
        cJSON_AddStringToObject(item, "key", my_tasks[i].key);
        cJSON_AddNumberToObject(item, "stack_cfg", my_tasks[i].usStackDepth);
        cJSON_AddBoolToObject(item, "active", (current_bits & my_tasks[i].event_bit) != 0);
        cJSON_AddNumberToObject(item, "delay_min", my_tasks[i].delay_ms / 60000);

        // Récupération des données dynamiques réelles de FreeRTOS si la tâche est instanciée
        if (my_tasks[i].pxTask != NULL)
        {
            vTaskGetInfo(my_tasks[i].pxTask, &details, pdTRUE, eInvalid);

            cJSON_AddNumberToObject(item, "prio_curr", details.uxCurrentPriority);
            cJSON_AddNumberToObject(item, "stack_min_ever", details.usStackHighWaterMark); // Pile restante (High Water Mark)

            const char *st = "Inconnu";
            switch (details.eCurrentState)
            {
            case eRunning:   st = "Running";   break;
            case eReady:     st = "Ready";     break;
            case eBlocked:   st = "Blocked";   break;
            case eSuspended: st = "Suspended"; break;
            default:                           break;
            }
            cJSON_AddStringToObject(item, "state", st);
        }
        cJSON_AddItemToArray(root, item);
    }
    return root;
}

/**
 * @brief Modifie dynamiquement la période de scrutation d'une tâche spécifique.
 */
void tasks_set_delay(const char *key, uint32_t delay_ms)
{
    for (int i = 0; i < TASK_COUNT; i++)
    {
        if (strcmp(my_tasks[i].key, key) == 0)
        {
            my_tasks[i].delay_ms = delay_ms;
            ESP_LOGI(TAG, "Nouveau delai pour %s : %lu ms", key, (unsigned long)delay_ms);
            break;
        }
    }

    // Persistance de la modification sur la carte SD
    save_json_to_sdcard("/sdcard/tasks.json");
}

/**
 * @brief Accesseur vers le descripteur du groupe d'événements interne.
 */
EventGroupHandle_t tasks_get_event_group(void)
{
    return s_task_event_group;
}
