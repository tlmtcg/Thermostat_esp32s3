#include "task_registry.h"

#include "jeedom_task.h"
#include "sht31_task.h"
#include "wifi_manager.h"
#include "weather_store.h"
#include "weather_task.h"

task_info_t my_tasks[] = {
    {"Meteo", "weather", 8192, 5, BIT_WEATHER_EN, NULL, 15 * 60 * 1000},
    {"Jeedom", "jeedom", 4096, 5, BIT_JEEDOM_EN, NULL, 60 * 1000},
    {"NTP", "ntp", 4096, 5, BIT_NTP_EN, NULL, 60 * 1000},
    {"Led", "led", 4600, 5, BIT_LED_EN, NULL, 0},
    {"Storage", "storage", 8192, 10, BIT_STORAGE_EN, NULL, 0},
    {"Serial", "serial", 4096, 5, BIT_SERIAL_EN, NULL, 0},
    {"SHT31", "sht31", 4096, 5, BIT_SHT31_EN, NULL, 0},
};

const int TASK_COUNT = sizeof(my_tasks) / sizeof(task_info_t);

static bool task_registry_is_wifi_connected(void)
{
    return wifi_get_state() == WIFI_STATE_STA_CONNECTED;
}

static weather_task_config_t weather_task_config = {
    .event_group = NULL,
    .event_bit = BIT_WEATHER_EN,
    .delay_ms = &my_tasks[0].delay_ms,
    .is_wifi_connected = task_registry_is_wifi_connected,
    .store_set_all = weather_store_set_all,
};

static jeedom_task_config_t jeedom_task_config = {
    .event_group = NULL,
    .event_bit = BIT_JEEDOM_EN,
    .delay_ms = &my_tasks[1].delay_ms,
    .is_wifi_connected = task_registry_is_wifi_connected,
};

static sht31_task_config_t sht31_task_config = {
    .event_group = NULL,
    .event_bit = BIT_SHT31_EN,
    .delay_ms = &my_tasks[6].delay_ms,
};

task_registry_entry_t task_registry[] = {
    {&my_tasks[0], weather_update_task, &weather_task_config},
    {&my_tasks[1], jeedom_send_task, &jeedom_task_config},
    {&my_tasks[2], ntp_monitor_task, NULL},
    {&my_tasks[3], led_task, NULL},
    {&my_tasks[4], alert_storage_task, NULL},
    {&my_tasks[5], serial_task, NULL},
    {&my_tasks[6], sht31_task, &sht31_task_config},
};

const int TASK_REGISTRY_COUNT = sizeof(task_registry) / sizeof(task_registry_entry_t);

void task_registry_set_event_group(EventGroupHandle_t event_group)
{
    weather_task_config.event_group = event_group;
    jeedom_task_config.event_group = event_group;
    sht31_task_config.event_group = event_group;
}
