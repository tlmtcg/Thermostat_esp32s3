#include "i2c_manager.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "time_utils.h"

#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  LOG TAG                                                                   */
/* -------------------------------------------------------------------------- */

static const char *TAG = "I2C_MANAGER";

/* -------------------------------------------------------------------------- */
/*  GLOBAL I2C BUS                                                            */
/* -------------------------------------------------------------------------- */

i2c_master_bus_handle_t i2c_bus = NULL;

/* -------------------------------------------------------------------------- */
/*  SCAN JSON                                                                 */
/* -------------------------------------------------------------------------- */

static cJSON *i2c_devices_json = NULL;

/* -------------------------------------------------------------------------- */
/*  SCAN RESULT                                                               */
/* -------------------------------------------------------------------------- */

static i2c_scan_result_t scan_result = {0};

/* -------------------------------------------------------------------------- */
/*  FREE JSON                                                                 */
/* -------------------------------------------------------------------------- */

static void free_json(void)
{
    if (i2c_devices_json) {
        cJSON_Delete(i2c_devices_json);
        i2c_devices_json = NULL;
    }
}

/* -------------------------------------------------------------------------- */
/*  FREE SCAN RESULT                                                          */
/* -------------------------------------------------------------------------- */

static void free_scan(void)
{
    if (scan_result.addresses) {
        free(scan_result.addresses);
        scan_result.addresses = NULL;
    }

    scan_result.count = 0;
    memset(scan_result.timestamp, 0, sizeof(scan_result.timestamp));
}

/* -------------------------------------------------------------------------- */
/*  INIT BUS                                                                  */
/* -------------------------------------------------------------------------- */

esp_err_t i2c_manager_init(int sda, int scl, int freq)
{
    if (i2c_bus != NULL) {
        ESP_LOGW(TAG, "I2C déjà initialisé");
        return ESP_OK;
    }

    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Init I2C SDA=%d SCL=%d Freq=%d", sda, scl, freq);

    esp_err_t err = i2c_new_master_bus(&cfg, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init I2C: %s", esp_err_to_name(err));
        i2c_bus = NULL;
        return err;
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  DELETE BUS                                                                */
/* -------------------------------------------------------------------------- */

esp_err_t i2c_manager_deinit(void)
{
    if (!i2c_bus) {
        ESP_LOGW(TAG, "Bus I2C déjà supprimé");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Suppression bus I2C...");

    esp_err_t err = i2c_del_master_bus(i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur suppression bus: %s", esp_err_to_name(err));
        return err;
    }

    i2c_bus = NULL;

    ESP_LOGI(TAG, "Bus I2C supprimé");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  SAFE DEVICE PROBE                                                         */
/* -------------------------------------------------------------------------- */

bool i2c_device_exists(i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (!bus) {
        return false;
    }

    esp_err_t err = i2c_master_probe(bus, addr, 10);
    return err == ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  SCAN FULL I2C BUS                                                         */
/* -------------------------------------------------------------------------- */

esp_err_t i2c_manager_scan(void)
{
    if (!i2c_bus) {
        ESP_LOGE(TAG, "Bus I2C non initialisé");
        return ESP_ERR_INVALID_STATE;
    }

    free_json();
    free_scan();

    scan_result.addresses = malloc(128 * sizeof(uint8_t));
    if (!scan_result.addresses) {
        return ESP_ERR_NO_MEM;
    }

    time_utils_get_time_str(
        scan_result.timestamp,
        sizeof(scan_result.timestamp)
    );

    ESP_LOGI(TAG, "Début scan I2C...");

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t err = i2c_master_probe(i2c_bus, addr, 100);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device trouvé: 0x%02X", addr);
            scan_result.addresses[scan_result.count++] = addr;
        }
    }

    ESP_LOGI(TAG, "Scan terminé (%d devices)", (int)scan_result.count);

    i2c_devices_json = cJSON_CreateObject();
    if (!i2c_devices_json) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(i2c_devices_json, "timestamp", scan_result.timestamp);

    cJSON *arr = cJSON_AddArrayToObject(i2c_devices_json, "devices");
    if (!arr) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < scan_result.count; i++) {
        cJSON *obj = cJSON_CreateObject();
        if (!obj) {
            continue;
        }
        cJSON_AddNumberToObject(obj, "address", scan_result.addresses[i]);
        cJSON_AddItemToArray(arr, obj);
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  JSON GETTER                                                               */
/* -------------------------------------------------------------------------- */

cJSON *i2c_manager_get_devices_json(void)
{
    return i2c_devices_json;
}

/* -------------------------------------------------------------------------- */
/*  RAW RESULT GETTER                                                         */
/* -------------------------------------------------------------------------- */

const i2c_scan_result_t *i2c_manager_get_scan_result(void)
{
    return &scan_result;
}

i2c_master_bus_handle_t i2c_manager_get_bus(void)
{
    return i2c_bus;
}

