#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"

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

#include "driver/i2c_master.h" // 🔥 IMPORTANT ESP-IDF v6

static const char *TAG = "MAIN_APP";

#ifndef CONFIG_FILE
#define CONFIG_FILE "/sdcard/config.json"
#endif

// =======================
// INIT I2C BUS (ESP-IDF v6)
// =======================
// static void i2c_bus_init(void)
// {
//     i2c_master_bus_config_t bus_cfg = {
//         .i2c_port = I2C_NUM_0,
//         .sda_io_num = 8,
//         .scl_io_num = 9,
//         .clk_source = I2C_CLK_SRC_DEFAULT,
//         .glitch_ignore_cnt = 7,
//         .flags.enable_internal_pullup = true,
//     };

//     ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
//     ESP_LOGI(TAG, "I2C bus initialisé");
// }

void test_email();

// void app_main(void)
// {
//     ESP_LOGI(TAG, "Démarrage du système...");

//     // --- NVS ---
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
//     {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ESP_ERROR_CHECK(nvs_flash_init());
//     }

//     // --- SD CARD ---
//     if (init_sd_card(NULL) != ESP_OK)
//     {
//         ESP_LOGW(TAG, "SD non disponible");
//         return;
//     }

//     // --- CONFIG ---
//     cJSON *config_json = load_json_from_sdcard(CONFIG_FILE);
//     if (config_json)
//     {
//         ESP_LOGI(TAG, "Config chargée");
//         cJSON_Delete(config_json);
//     }
//     else
//     {
//         ESP_LOGW(TAG, "Génération config");
//         save_kconfig_to_sdcard(CONFIG_FILE);
//     }

//     led_storage_init();
//     led_init();

//     // --- WIFI ---
//     wifi_app_start();

//     // --- WEB SERVER ---
//     ESP_LOGI(TAG, "Démarrage serveur web...");
//     httpd_handle_t server = start_webserver();

//     if (!server)
//     {
//         ESP_LOGE(TAG, "Serveur Web FAIL");
//         return;
//     }

//     ESP_LOGI(TAG, "Serveur OK");

//     // --- TIME ---
//     time_utils_init();

//     // --- LED DB ---
//     led_db_print_status();

//     // --- HEATING ---
//     heating_init(&config);

//     // =======================
//     // 🔥 I2C INIT PROPRE
//     // =======================
//     i2c_bus_init();

//     // Scan I2C
//     i2c_manager_scan();

//     // FIX IMPORTANT : i2c_device_exists DOIT recevoir bus + addr
//     if (i2c_device_exists(i2c_bus, 0x44))
//     {
//         sht31_start(i2c_bus, 0x44);
//     }
//     else if (i2c_device_exists(i2c_bus, 0x45))
//     {
//         sht31_start(i2c_bus, 0x45);
//     }

//     if (i2c_device_exists(i2c_bus, 0x3C))
//     {
//         ESP_LOGI(TAG, "OLED SSD1306 détecté");

//         ssd1306_init(&oled, i2c_bus, 0x3C);

//         oled_service_init(i2c_bus);

//         oled_service_show_boot();

//     } else {
//         ESP_LOGW(TAG,"OLE SSD1306 non détecté");
//     }

//     // DEBUG JSON I2C
//     cJSON *devices_json = i2c_manager_get_devices_json();
//     if (devices_json)
//     {
//         char *json_str = cJSON_Print(devices_json);
//         free(json_str);
//     }
// }

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
            oled_service_init(i2c_manager_get_bus())
);

        oled_service_show_boot();
    }

    static ssd1306_t lcd;
    /* 1. Init app context */
    app_context_init();

    /* 2. Init SSD1306 */
    ssd1306_init(&lcd, i2c_manager_get_bus(),0x3C);

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

    heating_init(&config);

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
