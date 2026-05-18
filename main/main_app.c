#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"

#include "web_server.h"
#include "wifi_app.h"
#include "time_utils.h"
#include "led_ctrl.h"
#include "esp_littlefs.h"
#include "alert_manager.h"
#include "task_manager.h"
#include "serial_manager.h"
#include "freebox_ftp.h"
#include "heating_program.h"
#include "sd_card.h"
#include "alert_storage.h"
#include "led_storage.h"

#include "config_storage.h"
#include "i2c_manager.h"
#include "email_service.h"
#include "cJSON.h"
#include "sht31.h"
#include "ssd1306.h"
#include "oled_service.h"
#include "app_context.h"
#include "display_pages.h"
#include "thermostat.h"

#include "driver/i2c_master.h" // 🔥 IMPORTANT ESP-IDF v6

static const char *TAG = "MAIN_APP";

#ifndef CONFIG_FILE
#define CONFIG_FILE "/sdcard/config.json"
#endif

void test_email();

void check_ota_boot(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();

    esp_app_desc_t app_desc;
    esp_ota_get_partition_description(running, &app_desc);

    ESP_LOGI("BOOT", "Running firmware: %s", app_desc.version);

    // IMPORTANT : si crash après OTA, rollback possible automatiquement
}

void app_main(void)
{
    ESP_LOGI(TAG, "Démarrage du système...");

    /* ------------------------------------------------------- */
    /* NVS */
    /* ------------------------------------------------------- */

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 🔥 Vérification OTA AVANT le reste
    check_ota_boot();

    // 2. Thermostat (charge config depuis NVS)
    thermostat_init();

    /* ------------------------------------------------------- */
    /* I2C EARLY INIT */
    /* ------------------------------------------------------- */

    ESP_ERROR_CHECK(
        i2c_manager_init(
            CONFIG_I2C_MANAGER_SDA,
            CONFIG_I2C_MANAGER_SCL,
            CONFIG_I2C_MANAGER_FREQ));

    /* ------------------------------------------------------- */
    /* OLED EARLY INIT */
    /* ------------------------------------------------------- */

    if (i2c_device_exists(i2c_manager_get_bus(), 0x3C))
    {
        ESP_LOGI(TAG, "OLED SSD1306 détecté");

        ESP_ERROR_CHECK(
            oled_service_init(i2c_manager_get_bus()));

        oled_service_show_boot();
    }

    static ssd1306_t lcd;
    /* 1. Init app context */
    app_context_init();

    /* 2. Init SSD1306 */
    ssd1306_init(&lcd, i2c_manager_get_bus(), 0x3C);

    /* 3. Donner le LCD au module UI */
    display_pages_init(&lcd);

    /* 4. Lancer la task d’affichage */
    display_pages_start();

    /* ------------------------------------------------------- */
    /* SD CARD */
    /* ------------------------------------------------------- */

    oled_service_show_text(
        "THERMOSTAT",
        "Mount SD...",
        NULL);

    if (init_sd_card(NULL) != ESP_OK)
    {
        ESP_LOGW(TAG, "SD non disponible");

        oled_service_show_error("SD FAIL");

        return;
    }

    /* ------------------------------------------------------- */
    /* CONFIG */
    /* ------------------------------------------------------- */

    oled_service_show_text(
        "THERMOSTAT",
        "Load config...",
        NULL);

    cJSON *config_json =
        load_json_from_sdcard(CONFIG_FILE);

    if (config_json)
    {
        ESP_LOGI(TAG, "Config chargée");
        cJSON_Delete(config_json);
    }
    else
    {
        ESP_LOGW(TAG, "Génération config");

        save_kconfig_to_sdcard(CONFIG_FILE);
    }

    /* ------------------------------------------------------- */
    /* LED */
    /* ------------------------------------------------------- */

    led_storage_init();
    led_init();

    /* ------------------------------------------------------- */
    /* WIFI */
    /* ------------------------------------------------------- */

    oled_service_show_text(
        "THERMOSTAT",
        "Starting WiFi...",
        NULL);

    wifi_app_start();

    /* ------------------------------------------------------- */
    /* WEB SERVER */
    /* ------------------------------------------------------- */

    oled_service_show_text(
        "THERMOSTAT",
        "Starting Web...",
        NULL);

    httpd_handle_t server =
        start_webserver();

    if (!server)
    {
        ESP_LOGE(TAG, "Serveur Web FAIL");

        oled_service_show_error(
            "WEB FAIL");

        return;
    }

    /* ------------------------------------------------------- */
    /* TIME */
    /* ------------------------------------------------------- */

    time_utils_init();

    /* ------------------------------------------------------- */
    /* HEATING */
    /* ------------------------------------------------------- */

    heating_init();

    /* ------------------------------------------------------- */
    /* I2C SCAN */
    /* ------------------------------------------------------- */

    i2c_manager_scan();

    /* ------------------------------------------------------- */
    /* SHT31 */
    /* ------------------------------------------------------- */

    if (i2c_device_exists(i2c_manager_get_bus(), 0x44))
    {
        sht31_start(i2c_manager_get_bus(), 0x44);
    }

    task_manager_init();
    /* ------------------------------------------------------- */
    /* READY */
    /* ------------------------------------------------------- */

    oled_service_show_text(
        "THERMOSTAT",
        "System Ready",
        NULL);

    ESP_LOGI(TAG, "Boot terminé");
}

// =======================
// EMAIL TEST
// =======================
void test_email()
{
    const char *target_file = "/sdcard/alerts.log";

    ESP_LOGI(TAG, "Envoi email...");
    email_send_log_async(
        "dup.cordon@gmail.com",
        "ESP32 Logs",
        "Ceci est le corps du mail",
        target_file);
}
