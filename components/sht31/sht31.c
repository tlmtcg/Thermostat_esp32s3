#include "sht31.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <time.h>
#include "oled_service.h"

/* -------------------------------------------------------------------------- */
/*  LOG TAG                                                                    */
/* -------------------------------------------------------------------------- */

static const char *TAG = "SHT31";

/* -------------------------------------------------------------------------- */
/*  COMMANDES SHT31                                                            */
/* -------------------------------------------------------------------------- */

#define SHT31_CMD_MEAS_HIGHREP   0x2400
#define SHT31_CMD_SOFT_RESET     0x30A2

/* -------------------------------------------------------------------------- */
/*  DRIVER INTERNAL CONTEXT                                                    */
/* -------------------------------------------------------------------------- */

typedef struct {

    /* handle device I2C */
    i2c_master_dev_handle_t dev;

    /* adresse I2C */
    uint8_t addr;

    /* init state */
    bool initialized;

    /* task running */
    bool running;

    /* FreeRTOS task */
    TaskHandle_t task;

    /* public sensor state */
    sht31_state_t state;

} sht31_ctx_t;

/* -------------------------------------------------------------------------- */
/*  GLOBAL DRIVER INSTANCE                                                     */
/* -------------------------------------------------------------------------- */

static sht31_ctx_t g_sht31 = {0};

/* -------------------------------------------------------------------------- */
/*  CRC8 SHT31                                                                 */
/* -------------------------------------------------------------------------- */

static uint8_t sht31_crc8(const uint8_t *data,
                          int len)
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
/*  WRITE COMMAND                                                              */
/* -------------------------------------------------------------------------- */

static esp_err_t sht31_write_cmd(uint16_t cmd)
{
    if (!g_sht31.dev)
        return ESP_ERR_INVALID_STATE;

    uint8_t data[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF)
    };

    return i2c_master_transmit(
        g_sht31.dev,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100)
    );
}

/* -------------------------------------------------------------------------- */
/*  GET PUBLIC STATE                                                           */
/* -------------------------------------------------------------------------- */

const sht31_state_t *sht31_get_state(void)
{
    return &g_sht31.state;
}

/* -------------------------------------------------------------------------- */
/*  INIT DEVICE                                                                */
/* -------------------------------------------------------------------------- */

esp_err_t sht31_init(i2c_master_bus_handle_t bus,
                     uint8_t addr)
{
    if (!bus)
        return ESP_ERR_INVALID_ARG;

    if (g_sht31.initialized) {

        ESP_LOGW(TAG, "SHT31 déjà initialisé");

        return ESP_OK;
    }

    memset(&g_sht31, 0, sizeof(g_sht31));

    g_sht31.addr = addr;

    /* configuration device I2C */
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };

    /* attach device to bus */
    esp_err_t err = i2c_master_bus_add_device(
        bus,
        &cfg,
        &g_sht31.dev
    );

    if (err != ESP_OK) {

        ESP_LOGE(TAG,
                 "Erreur add device: %s",
                 esp_err_to_name(err));

        return err;
    }

    g_sht31.initialized = true;

    ESP_LOGI(TAG,
             "SHT31 initialisé @0x%02X",
             addr);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  REMOVE DEVICE                                                              */
/* -------------------------------------------------------------------------- */

void sht31_deinit(void)
{
    /* stop task first */
    sht31_stop();

    /* remove device from I2C bus */
    if (g_sht31.dev) {

        i2c_master_bus_rm_device(
            g_sht31.dev
        );

        g_sht31.dev = NULL;
    }

    memset(&g_sht31, 0, sizeof(g_sht31));

    ESP_LOGI(TAG, "SHT31 deinit");
}

/* -------------------------------------------------------------------------- */
/*  SOFT RESET                                                                 */
/* -------------------------------------------------------------------------- */

esp_err_t sht31_reset(void)
{
    return sht31_write_cmd(
        SHT31_CMD_SOFT_RESET
    );
}

/* -------------------------------------------------------------------------- */
/*  SINGLE READ                                                                */
/* -------------------------------------------------------------------------- */

esp_err_t sht31_read(float *temp,
                     float *hum)
{
    if (!g_sht31.dev)
        return ESP_ERR_INVALID_STATE;

    if (!temp || !hum)
        return ESP_ERR_INVALID_ARG;

    uint8_t rx[6];

    /* lancer mesure */
    esp_err_t err = sht31_write_cmd(
        SHT31_CMD_MEAS_HIGHREP
    );

    if (err != ESP_OK)
        return err;

    /* temps conversion */
    vTaskDelay(pdMS_TO_TICKS(20));

    /* lecture résultat */
    err = i2c_master_receive(
        g_sht31.dev,
        rx,
        sizeof(rx),
        pdMS_TO_TICKS(100)
    );

    if (err != ESP_OK)
        return err;

    /* vérification CRC température */
    if (sht31_crc8(&rx[0], 2) != rx[2]) {

        ESP_LOGE(TAG, "CRC température invalide");

        return ESP_ERR_INVALID_CRC;
    }

    /* vérification CRC humidité */
    if (sht31_crc8(&rx[3], 2) != rx[5]) {

        ESP_LOGE(TAG, "CRC humidité invalide");

        return ESP_ERR_INVALID_CRC;
    }

    /* conversion raw */
    uint16_t raw_t =
        ((uint16_t)rx[0] << 8) | rx[1];

    uint16_t raw_h =
        ((uint16_t)rx[3] << 8) | rx[4];

    *temp =
        -45.0f +
        (175.0f * ((float)raw_t / 65535.0f));

    *hum =
        100.0f *
        ((float)raw_h / 65535.0f);

    /* update public state */
    g_sht31.state.temperature = *temp;
    g_sht31.state.humidity = *hum;
    g_sht31.state.valid = true;
    g_sht31.state.last_update = time(NULL);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  BACKGROUND TASK                                                            */
/* -------------------------------------------------------------------------- */

static void sht31_task(void *arg)
{
    float t;
    float h;

    while (g_sht31.running) {

        if (!sht31_read(&t, &h) == ESP_OK) {
            ESP_LOGW(TAG,
                     "Lecture SHT31 échouée");
            oled_service_show_error("SHT31 ERROR");
            g_sht31.state.valid = false;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    g_sht31.task = NULL;

    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*  START TASK                                                                 */
/* -------------------------------------------------------------------------- */

esp_err_t sht31_start(i2c_master_bus_handle_t bus,
                      uint8_t addr)
{
    if (g_sht31.running) {

        ESP_LOGW(TAG,
                 "Task déjà démarrée");

        return ESP_OK;
    }

    esp_err_t err = sht31_init(
        bus,
        addr
    );

    if (err != ESP_OK)
        return err;

    g_sht31.running = true;

    BaseType_t ok = xTaskCreate(
        sht31_task,
        "sht31_task",
        4096,
        NULL,
        5,
        &g_sht31.task
    );

    if (ok != pdPASS) {

        sht31_deinit();

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Task SHT31 démarrée");

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  STOP TASK                                                                  */
/* -------------------------------------------------------------------------- */

void sht31_stop(void)
{
    g_sht31.running = false;

    if (g_sht31.task) {

        vTaskDelete(
            g_sht31.task
        );

        g_sht31.task = NULL;
    }

    ESP_LOGI(TAG, "Task SHT31 stoppée");
}
