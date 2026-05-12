#include "i2c_manager.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "time_utils.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "I2C_MANAGER";

/* =========================
 *  BUS UNIQUE GLOBAL
 * ========================= */
i2c_master_bus_handle_t i2c_bus = NULL;

/* =========================
 *  JSON + SCAN RESULT
 * ========================= */
static cJSON *i2c_devices_json = NULL;

static i2c_scan_result_t scan_result = {0};

/* =========================
 *  CLEANUP
 * ========================= */
static void free_json(void)
{
    if (i2c_devices_json) {
        cJSON_Delete(i2c_devices_json);
        i2c_devices_json = NULL;
    }
}

static void free_scan(void)
{
    if (scan_result.addresses) {
        free(scan_result.addresses);
        scan_result.addresses = NULL;
    }
    scan_result.count = 0;
}

/* =========================
 *  INIT I2C BUS (ESP-IDF v6)
 * ========================= */
esp_err_t i2c_manager_init(void)
{
    if (i2c_bus != NULL) {
        ESP_LOGW(TAG, "I2C déjà initialisé");
        return ESP_OK;
    }

    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_I2C_MANAGER_SDA,
        .scl_io_num = CONFIG_I2C_MANAGER_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Init I2C SDA=%d SCL=%d Freq=%d",
             cfg.sda_io_num,
             cfg.scl_io_num,
             CONFIG_I2C_MANAGER_FREQ);

    esp_err_t err = i2c_new_master_bus(&cfg, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init I2C: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/* =========================
 *  DEVICE EXISTS (safe probe)
 * ========================= */
bool i2c_device_exists(i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (!bus) return false;

    i2c_master_dev_handle_t dev;

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = CONFIG_I2C_MANAGER_FREQ,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &dev);

    if (err == ESP_OK) {
        i2c_master_bus_rm_device(dev);
        return true;
    }

    return false;
}

/* =========================
 *  SCAN I2C
 * ========================= */
esp_err_t i2c_manager_scan(void)
{
    if (!i2c_bus) {
        ESP_LOGE(TAG, "Bus I2C non initialisé");
        return ESP_ERR_INVALID_STATE;
    }

    free_json();
    free_scan();

    scan_result.addresses = malloc(128);
    if (!scan_result.addresses)
        return ESP_ERR_NO_MEM;

    time_utils_get_time_str(scan_result.timestamp, sizeof(scan_result.timestamp));

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {

        i2c_master_dev_handle_t dev;

        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = CONFIG_I2C_MANAGER_FREQ,
        };

        if (i2c_master_bus_add_device(i2c_bus, &cfg, &dev) == ESP_OK) {
            scan_result.addresses[scan_result.count++] = addr;
            i2c_master_bus_rm_device(dev);
        }
    }

    /* JSON */
    i2c_devices_json = cJSON_CreateObject();
    cJSON_AddStringToObject(i2c_devices_json, "timestamp", scan_result.timestamp);

    cJSON *arr = cJSON_AddArrayToObject(i2c_devices_json, "devices");

    for (size_t i = 0; i < scan_result.count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "address", scan_result.addresses[i]);
        cJSON_AddItemToArray(arr, obj);
    }

    return ESP_OK;
}

/* =========================
 *  GETTERS
 * ========================= */
cJSON *i2c_manager_get_devices_json(void)
{
    return i2c_devices_json;
}

const i2c_scan_result_t *i2c_manager_get_scan_result(void)
{
    return &scan_result;
}
