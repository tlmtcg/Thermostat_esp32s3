#include "wifi_manager.h"
#include "wifi_storage.h"
#include "wifi_mode.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include <string.h>
#include "sdkconfig.h"

static const char *TAG = "WIFI_MGR";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static TimerHandle_t s_wifi_retry_timer;
static int s_retry_num = 0;
static bool s_sta_connected = false;
static int s_ap_clients = 0;
static bool s_test_mode = false;

static void wifi_retry_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Retry STA...");
    esp_wifi_connect();
}

void wifi_manager_init(const wifi_callbacks_t *callbacks)
{
    ESP_LOGI(TAG, "Init WiFi Manager");

    s_wifi_event_group = xEventGroupCreate();
    s_wifi_retry_timer = xTimerCreate("wifi_retry",
                                      pdMS_TO_TICKS(3000),
                                      pdFALSE,
                                      NULL,
                                      wifi_retry_callback);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_events_init(callbacks);

    char ssid[33] = {0}, pass[65] = {0};
    if (!wifi_storage_load(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        strlcpy(ssid, CONFIG_WIFI_STA_DEFAULT_SSID, sizeof(ssid));
        strlcpy(pass, CONFIG_WIFI_STA_DEFAULT_PASSWORD, sizeof(pass));
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = CONFIG_WIFI_AP_SSID,
            .password = CONFIG_WIFI_AP_PASSWORD,
            .channel = CONFIG_WIFI_AP_CHANNEL,
            .max_connection = CONFIG_WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi démarré (AP+STA)");
}

wifi_state_t wifi_get_state(void)
{
    return wifi_state_get();
}

int wifi_get_ap_client_count(void)
{
    return s_ap_clients;
}

void wifi_manager_try_connect(const char *ssid, const char *pass)
{
    ESP_LOGI(TAG, "Connexion manuelle vers %s", ssid);

    s_test_mode = true;
    s_retry_num = 0;
    xTimerStop(s_wifi_retry_timer, 0);

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    if (pass)
        strlcpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password));

    sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connexion OK, sauvegarde NVS");
        wifi_storage_save(ssid, pass ? pass : "");
    }
    else
    {
        ESP_LOGE(TAG, "Échec connexion");
    }

    s_test_mode = false;
    wifi_mode_update(s_sta_connected, s_ap_clients, s_test_mode);
}
