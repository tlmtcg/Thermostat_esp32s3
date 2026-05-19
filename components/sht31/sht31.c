#include "sht31.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_manager.h"
#include "sdkconfig.h"

static const char *TAG = "SHT31";

#define SHT31_CMD_MEAS_HIGHREP 0x2400
#define SHT31_CMD_SOFT_RESET 0x30A2
#define SHT31_DEFAULT_ADDR 0x44
#define SHT31_DEFAULT_READ_INTERVAL_MS 5000
typedef struct
{
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    sht31_config_t config;
    sht31_runtime_t runtime;
} sht31_ctx_t;

static sht31_ctx_t g_sht31 = {
    .config = {
        .addr = SHT31_DEFAULT_ADDR,
        .read_interval_ms = SHT31_DEFAULT_READ_INTERVAL_MS,
        .log_to_sd = true,
    },
    .runtime = {
        .last_error = "",
    },
};

static uint8_t sht31_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }

    return crc;
}

static void sht31_set_error(const char *message)
{
    if (!message)
        message = "";

    snprintf(g_sht31.runtime.last_error,
             sizeof(g_sht31.runtime.last_error),
             "%s",
             message);
}

static void sht31_clear_error(void)
{
    g_sht31.runtime.last_error[0] = '\0';
}

static esp_err_t sht31_write_cmd(uint16_t cmd)
{
    if (!g_sht31.dev)
        return ESP_ERR_INVALID_STATE;

    uint8_t data[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF),
    };

    return i2c_master_transmit(g_sht31.dev,
                               data,
                               sizeof(data),
                               pdMS_TO_TICKS(200));
}

static esp_err_t sht31_attach_device(uint8_t addr)
{
    if (!g_sht31.bus)
        return ESP_ERR_INVALID_STATE;

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = CONFIG_I2C_MANAGER_FREQ,
    };

    return i2c_master_bus_add_device(g_sht31.bus, &cfg, &g_sht31.dev);
}

esp_err_t sht31_get_config(sht31_config_t *out)
{
    if (!out)
        return ESP_ERR_INVALID_ARG;

    *out = g_sht31.config;
    return ESP_OK;
}

esp_err_t sht31_set_config(const sht31_config_t *config)
{
    if (!config)
        return ESP_ERR_INVALID_ARG;

    if (config->addr == 0)
        return ESP_ERR_INVALID_ARG;

    sht31_config_t new_config = *config;

    if (new_config.read_interval_ms == 0)
        new_config.read_interval_ms = SHT31_DEFAULT_READ_INTERVAL_MS;

    bool addr_changed = new_config.addr != g_sht31.config.addr;

    if (addr_changed && g_sht31.runtime.initialized)
    {
        if (g_sht31.dev)
        {
            i2c_master_bus_rm_device(g_sht31.dev);
            g_sht31.dev = NULL;
        }

        esp_err_t err = sht31_attach_device(new_config.addr);
        if (err != ESP_OK)
        {
            g_sht31.runtime.valid = false;
            g_sht31.runtime.initialized = false;
            g_sht31.runtime.error_count++;
            sht31_set_error(esp_err_to_name(err));
            return err;
        }

        g_sht31.runtime.valid = false;
        sht31_clear_error();
    }

    g_sht31.config = new_config;

    ESP_LOGI(TAG,
             "Config appliquee (addr=0x%02X, interval=%u ms, log_to_sd=%d)",
             g_sht31.config.addr,
             (unsigned)g_sht31.config.read_interval_ms,
             g_sht31.config.log_to_sd);

    return ESP_OK;
}

const sht31_runtime_t *sht31_get_runtime(void)
{
    return &g_sht31.runtime;
}

void sht31_set_running(bool running)
{
    g_sht31.runtime.running = running;
}

char *sht31_get_json_status(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddNumberToObject(runtime, "temperature", g_sht31.runtime.temperature);
    cJSON_AddNumberToObject(runtime, "humidity", g_sht31.runtime.humidity);
    cJSON_AddBoolToObject(runtime, "valid", g_sht31.runtime.valid);
    cJSON_AddNumberToObject(runtime, "last_update", (double)g_sht31.runtime.last_update);
    cJSON_AddBoolToObject(runtime, "initialized", g_sht31.runtime.initialized);
    cJSON_AddBoolToObject(runtime, "running", g_sht31.runtime.running);
    cJSON_AddNumberToObject(runtime, "read_count", g_sht31.runtime.read_count);
    cJSON_AddNumberToObject(runtime, "error_count", g_sht31.runtime.error_count);
    cJSON_AddStringToObject(runtime, "last_error", g_sht31.runtime.last_error);

    cJSON *config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddNumberToObject(config, "addr", g_sht31.config.addr);
    cJSON_AddNumberToObject(config, "read_interval_ms", g_sht31.config.read_interval_ms);
    cJSON_AddBoolToObject(config, "log_to_sd", g_sht31.config.log_to_sd);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

const sht31_state_t *sht31_get_state(void)
{
    return &g_sht31.runtime;
}

esp_err_t sht31_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (!bus)
        return ESP_ERR_INVALID_ARG;

    if (addr == 0)
        return ESP_ERR_INVALID_ARG;

    if (g_sht31.runtime.initialized)
    {
        ESP_LOGW(TAG, "SHT31 deja initialise");
        return ESP_OK;
    }

    g_sht31.bus = bus;
    g_sht31.config.addr = addr;

    esp_err_t err = sht31_attach_device(g_sht31.config.addr);
    if (err != ESP_OK)
    {
        g_sht31.runtime.error_count++;
        sht31_set_error(esp_err_to_name(err));
        ESP_LOGE(TAG, "Erreur add device: %s", esp_err_to_name(err));
        return err;
    }

    g_sht31.runtime.initialized = true;
    sht31_clear_error();

    ESP_LOGI(TAG, "SHT31 initialise @0x%02X", g_sht31.config.addr);
    return ESP_OK;
}

void sht31_deinit(void)
{
    sht31_stop();

    if (g_sht31.dev)
    {
        i2c_master_bus_rm_device(g_sht31.dev);
        g_sht31.dev = NULL;
    }

    g_sht31.bus = NULL;
    g_sht31.runtime.initialized = false;
    g_sht31.runtime.valid = false;

    ESP_LOGI(TAG, "SHT31 deinit");
}

esp_err_t sht31_reset(void)
{
    return sht31_write_cmd(SHT31_CMD_SOFT_RESET);
}

esp_err_t sht31_read(float *temp, float *hum)
{
    if (!g_sht31.dev)
    {
        g_sht31.runtime.valid = false;
        g_sht31.runtime.error_count++;
        sht31_set_error(esp_err_to_name(ESP_ERR_INVALID_STATE));
        return ESP_ERR_INVALID_STATE;
    }

    if (!temp || !hum)
        return ESP_ERR_INVALID_ARG;

    uint8_t rx[6];
    esp_err_t err = sht31_write_cmd(SHT31_CMD_MEAS_HIGHREP);
    if (err != ESP_OK)
        goto fail;

    vTaskDelay(pdMS_TO_TICKS(25));

    err = i2c_master_receive(g_sht31.dev,
                             rx,
                             sizeof(rx),
                             pdMS_TO_TICKS(500));
    if (err != ESP_OK)
        goto fail;

    if (sht31_crc8(&rx[0], 2) != rx[2])
    {
        err = ESP_ERR_INVALID_CRC;
        goto fail;
    }

    if (sht31_crc8(&rx[3], 2) != rx[5])
    {
        err = ESP_ERR_INVALID_CRC;
        goto fail;
    }

    uint16_t raw_t = ((uint16_t)rx[0] << 8) | rx[1];
    uint16_t raw_h = ((uint16_t)rx[3] << 8) | rx[4];

    *temp = -45.0f + (175.0f * ((float)raw_t / 65535.0f));
    *hum = 100.0f * ((float)raw_h / 65535.0f);

    g_sht31.runtime.temperature = *temp;
    g_sht31.runtime.humidity = *hum;
    g_sht31.runtime.valid = true;
    g_sht31.runtime.last_update = time(NULL);
    g_sht31.runtime.read_count++;
    sht31_clear_error();

    return ESP_OK;

fail:
    g_sht31.runtime.valid = false;
    g_sht31.runtime.error_count++;
    sht31_set_error(esp_err_to_name(err));
    ESP_LOGW(TAG, "Lecture SHT31 echouee: %s", esp_err_to_name(err));
    return err;
}

esp_err_t sht31_start(i2c_master_bus_handle_t bus, uint8_t addr)
{
    esp_err_t err = sht31_init(bus, addr);
    if (err != ESP_OK)
        return err;

    g_sht31.runtime.running = true;

    ESP_LOGI(TAG, "SHT31 actif");
    return ESP_OK;
}

void sht31_stop(void)
{
    g_sht31.runtime.running = false;

    ESP_LOGI(TAG, "SHT31 stoppe");
}
