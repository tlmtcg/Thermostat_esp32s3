#include "sht31.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>
#include <time.h>

static const char *TAG = "SHT31";

/* -------------------------------------------------------------------------- */
/*  COMMANDES SHT31                                                           */
/* -------------------------------------------------------------------------- */
#define SHT31_CMD_MEAS_HIGHREP  0x2400
#define SHT31_CMD_SOFT_RESET    0x30A2

/* -------------------------------------------------------------------------- */
/*  STATE GLOBAL                                                              */
/* -------------------------------------------------------------------------- */
static sht31_state_t g_sht31;
static sht31_t g_dev;

/* -------------------------------------------------------------------------- */
/*  GET STATE (pour API web)                                                  */
/* -------------------------------------------------------------------------- */
const sht31_state_t *sht31_get_state(void)
{
    return &g_sht31;
}

/* -------------------------------------------------------------------------- */
/*  CRC8 SHT31 (polynôme 0x31)                                                */
/* -------------------------------------------------------------------------- */
static uint8_t sht31_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < len; i++) {
        crc ^= data[i];

        for (int b = 0; b < 8; b++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }

    return crc;
}

/* -------------------------------------------------------------------------- */
/*  WRITE COMMAND (I2C modern driver)                                         */
/* -------------------------------------------------------------------------- */
static esp_err_t sht31_write_cmd(i2c_master_dev_handle_t dev, uint16_t cmd)
{
    uint8_t data[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF)
    };

    return i2c_master_transmit(dev, data, 2, pdMS_TO_TICKS(1000));
}

/* -------------------------------------------------------------------------- */
/*  INIT SHT31                                                                */
/*  - crée device I2C (ESP-IDF v6)                                            */
/* -------------------------------------------------------------------------- */
esp_err_t sht31_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    memset(&g_dev, 0, sizeof(g_dev));

    g_dev.addr = addr;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(
        bus,
        &dev_cfg,
        &g_dev.dev
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur création device I2C");
        return err;
    }

    g_sht31.valid = false;

    ESP_LOGI(TAG, "SHT31 initialisé à 0x%02X", addr);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  SOFT RESET                                                                */
/* -------------------------------------------------------------------------- */
esp_err_t sht31_reset(void)
{
    return sht31_write_cmd(g_dev.dev, SHT31_CMD_SOFT_RESET);
}

/* -------------------------------------------------------------------------- */
/*  LECTURE TEMP + HUM (CRC vérifié)                                          */
/* -------------------------------------------------------------------------- */
esp_err_t sht31_read(float *temp, float *hum)
{
    uint8_t rx[6];

    /* --- lancer mesure --- */
    esp_err_t err = sht31_write_cmd(g_dev.dev, SHT31_CMD_MEAS_HIGHREP);
    if (err != ESP_OK)
        return err;

    vTaskDelay(pdMS_TO_TICKS(20));

    /* --- lecture 6 bytes --- */
    err = i2c_master_receive(
        g_dev.dev,
        rx,
        sizeof(rx),
        pdMS_TO_TICKS(1000)
    );

    if (err != ESP_OK)
        return err;

    /* --- CRC check --- */
    if (sht31_crc8(&rx[0], 2) != rx[2]) return ESP_FAIL;
    if (sht31_crc8(&rx[3], 2) != rx[5]) return ESP_FAIL;

    /* --- conversion raw -> valeurs --- */
    uint16_t raw_t = (rx[0] << 8) | rx[1];
    uint16_t raw_h = (rx[3] << 8) | rx[4];

    *temp = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *hum  = 100.0f * ((float)raw_h / 65535.0f);

    /* --- update state global --- */
    g_sht31.temperature = *temp;
    g_sht31.humidity = *hum;
    g_sht31.valid = true;
    g_sht31.last_update = time(NULL);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  TASK FREE RTOS (non bloquant système)                                     */
/* -------------------------------------------------------------------------- */
static void sht31_task(void *arg)
{
    float t, h;

    while (1) {

        if (sht31_read(&t, &h) == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %.2f°C Hum: %.2f%%", t, h);
        } else {
            ESP_LOGW(TAG, "Lecture SHT31 échouée");
            g_sht31.valid = false;
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* -------------------------------------------------------------------------- */
/*  START (lancement task)                                                    */
/* -------------------------------------------------------------------------- */
void sht31_start(i2c_master_bus_handle_t bus, uint8_t addr)
{
    ESP_LOGI(TAG, "Démarrage SHT31...");

    if (sht31_init(bus, addr) != ESP_OK) {
        ESP_LOGE(TAG, "Init SHT31 échouée");
        return;
    }

    xTaskCreate(
        sht31_task,
        "sht31_task",
        4096,
        NULL,
        5,
        NULL
    );
}
