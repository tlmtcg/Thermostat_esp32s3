#include <stdio.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "alert_manager.h"
#include "alert_storage.h"
#include "app_context.h"
#include "cJSON.h"
#include "config_storage.h"
#include "driver/i2c_master.h"
#include "email_service.h"
#include "freebox_ftp.h"
#include "heating_program.h"
#include "i2c_manager.h"
#include "led_ctrl.h"
#include "oled_service.h"
#include "sd_card.h"
#include "serial_manager.h"
#include "sht31.h"
#include "ssd1306.h"
#include "tasks.h"
#include "thermostat.h"
#include "time_utils.h"
#include "web_server.h"
#include "wifi_app.h"
#include "prediction_engine.h"
#include "thermal_model.h"
#include "rc_estimator.h"

static const char *TAG = "MAIN_APP";

#ifndef CONFIG_FILE
#define CONFIG_FILE "/sdcard/config.json"
#endif

void test_email(void);
void check_ota_boot(void);

void check_ota_boot(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();

    esp_app_desc_t app_desc;
    esp_ota_get_partition_description(running, &app_desc);

    ESP_LOGI("BOOT", "Running firmware: %s", app_desc.version);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Demarrage du systeme...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    check_ota_boot();


    ESP_ERROR_CHECK(
        i2c_manager_init(
            CONFIG_I2C_MANAGER_SDA,
            CONFIG_I2C_MANAGER_SCL,
            CONFIG_I2C_MANAGER_FREQ));

    app_context_init();

    if (i2c_device_exists(i2c_manager_get_bus(), 0x3C))
    {
        ESP_LOGI(TAG, "OLED SSD1306 detecte");
        ESP_ERROR_CHECK(oled_service_init(i2c_manager_get_bus()));
        oled_service_show_boot();
    }

    oled_service_show_text("THERMOSTAT", "Mount SD...", NULL);

    if (init_sd_card(NULL) != ESP_OK)
    {
        ESP_LOGW(TAG, "SD non disponible");
        oled_service_show_error("SD FAIL");
        // return;
    }

    oled_service_show_text("THERMOSTAT", "Load config...", NULL);

    cJSON *config_json = load_json_from_sdcard(CONFIG_FILE);
    if (config_json)
    {
        ESP_LOGI(TAG, "Config chargee");
        cJSON_Delete(config_json);
    }
    else
    {
        ESP_LOGW(TAG, "Generation config");
        save_kconfig_to_sdcard(CONFIG_FILE);
    }

    led_init();

    oled_service_show_text("THERMOSTAT", "Starting WiFi...", NULL);
    wifi_app_start();

    oled_service_show_text("THERMOSTAT", "Starting Web...", NULL);

    httpd_handle_t server = start_webserver();
    if (!server)
    {
        ESP_LOGE(TAG, "Serveur Web FAIL");
        oled_service_show_error("WEB FAIL");
        return;
    }

    time_utils_init();
    heating_init();
    i2c_manager_scan();

    if (i2c_device_exists(i2c_manager_get_bus(), 0x44))
    {
        sht31_start(i2c_manager_get_bus(), 0x44);
    }

    tasks_init();
    thermostat_init();
    
    oled_service_show_text("THERMOSTAT", "System Ready", NULL);
    oled_service_start();

    app_init_thermal_model();
    prediction_engine_init();

    ESP_LOGI(TAG, "Boot termine");
}

void test_email(void)
{
    const char *target_file = "/sdcard/alerts.log";

    ESP_LOGI(TAG, "Envoi email...");
    email_send_log_async(
        "dup.cordon@gmail.com",
        "ESP32 Logs",
        "Ceci est le corps du mail",
        target_file);
}
