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
#include "thermal_engine.h"
#include "config_runtime.h"

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

    // --- Charger la config runtime (NVS) ---
    config_runtime_load(); // 🔥 NOUVEAU : charge ville, lat/lon, thermostat, calibration, etc.

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

    // Utilisation de l'adresse dynamique chargée depuis la NVS
    uint8_t target_sht_addr = g_cfg.sht31_addr;

    if (i2c_device_exists(i2c_manager_get_bus(), target_sht_addr))
    {
        ESP_LOGI(TAG, "Capteur SHT31 détecté à l'adresse 0x%02X", target_sht_addr);

        // 1. On initialise le driver SHT31 avec le bus et l'adresse NVS
        // (C'est ici que g_sht31.config prend l'intervalle et le flag SD !)
        if (sht31_init(i2c_manager_get_bus(), target_sht_addr) == ESP_OK)
        {
            // 2. On démarre la tâche ou les services liés au SHT31
            // Si ta fonction d'origine s'appelait sht31_start, ajuste son contenu
            // pour qu'elle ne ré-initialise pas tout, ou remplace-la si nécessaire.
            sht31_start(i2c_manager_get_bus(), target_sht_addr);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Capteur SHT31 INTROUVABLE à l'adresse NVS: 0x%02X", target_sht_addr);
        alert_add("Capteur SHT31 absent au boot");
    }

    tasks_init();
    thermostat_init();
    prediction_engine_init();

    oled_service_show_text("THERMOSTAT", "System Ready", NULL);
    oled_service_start();

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
