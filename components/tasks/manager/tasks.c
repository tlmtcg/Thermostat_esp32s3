#include "tasks.h"
#include "task_registry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include <string.h>
#include <time.h>

#include "weather_store.h"
#include "esp_sntp.h"
#include "alert_storage.h"
#include "serial_manager.h"
#include "wifi_manager.h"
#include "config_storage.h"

static const char *TAG = "TASKS";
static EventGroupHandle_t s_task_event_group;

static void tasks_apply_kconfig(void)
{
    xEventGroupClearBits(s_task_event_group, 0xFFFFFF);

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

    xEventGroupSetBits(s_task_event_group, BIT_STORAGE_EN);
}

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

        for (int i = 0; i < TASK_COUNT; i++)
        {
            if (strcmp(my_tasks[i].key, id->valuestring) == 0)
            {
                if (cJSON_IsNumber(delay))
                    my_tasks[i].delay_ms = delay->valueint;

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

    xEventGroupSetBits(s_task_event_group, BIT_STORAGE_EN);
}

void ntp_monitor_task(void *pvParameters)
{
    while (1)
    {
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

        vTaskDelay(pdMS_TO_TICKS(my_tasks[2].delay_ms));
    }
}

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

void tasks_init(void)
{
    s_task_event_group = xEventGroupCreate();

    serial_manager_init();
    weather_store_init();
    alert_storage_init("/sdcard/alerts.log");

    tasks_apply_kconfig();
    task_registry_set_event_group(s_task_event_group);

    cJSON *json = load_json_from_sdcard("/sdcard/tasks.json");
    if (json)
    {
        ESP_LOGI(TAG, "Configuration JSON trouvee sur SD. Application des parametres...");
        tasks_apply_json(json);
        cJSON_Delete(json);
    }

    tasks_dump_config();

    for (int i = 0; i < TASK_REGISTRY_COUNT; i++)
    {
        task_registry_entry_t *task = &task_registry[i];
        void *parameter = task->parameter;

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

void tasks_set_active(uint32_t bit, bool active)
{
    if (s_task_event_group == NULL)
        return;

    if (active)
        xEventGroupSetBits(s_task_event_group, bit);
    else
        xEventGroupClearBits(s_task_event_group, bit);

    save_json_to_sdcard("/sdcard/tasks.json");
}

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

        cJSON_AddStringToObject(item, "name", my_tasks[i].pcName);
        cJSON_AddStringToObject(item, "key", my_tasks[i].key);
        cJSON_AddNumberToObject(item, "stack_cfg", my_tasks[i].usStackDepth);
        cJSON_AddBoolToObject(item, "active", (current_bits & my_tasks[i].event_bit) != 0);
        cJSON_AddNumberToObject(item, "delay_min", my_tasks[i].delay_ms / 60000);

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

    save_json_to_sdcard("/sdcard/tasks.json");
}

EventGroupHandle_t tasks_get_event_group(void)
{
    return s_task_event_group;
}
