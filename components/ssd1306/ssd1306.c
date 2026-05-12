#include "ssd1306.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SSD1306";

#define SSD1306_CONTROL_CMD   0x00
#define SSD1306_CONTROL_DATA  0x40

// -------------------------
// Font minimal 5x7
// -------------------------

static const uint8_t font5x7[][5] = {
    ['0'] = {0x3E,0x51,0x49,0x45,0x3E},
    ['1'] = {0x00,0x42,0x7F,0x40,0x00},
    ['2'] = {0x42,0x61,0x51,0x49,0x46},
    ['3'] = {0x21,0x41,0x45,0x4B,0x31},
    ['4'] = {0x18,0x14,0x12,0x7F,0x10},
    ['5'] = {0x27,0x45,0x45,0x45,0x39},
    ['6'] = {0x3C,0x4A,0x49,0x49,0x30},
    ['7'] = {0x01,0x71,0x09,0x05,0x03},
    ['8'] = {0x36,0x49,0x49,0x49,0x36},
    ['9'] = {0x06,0x49,0x49,0x29,0x1E},

    ['A'] = {0x7E,0x11,0x11,0x11,0x7E},
    ['B'] = {0x7F,0x49,0x49,0x49,0x36},
    ['C'] = {0x3E,0x41,0x41,0x41,0x22},
    ['D'] = {0x7F,0x41,0x41,0x22,0x1C},
    ['E'] = {0x7F,0x49,0x49,0x49,0x41},
};

static esp_err_t ssd1306_write_cmd(ssd1306_t *lcd, uint8_t cmd)
{
    uint8_t data[2] = {
        SSD1306_CONTROL_CMD,
        cmd
    };

    return i2c_master_transmit(
        lcd->dev,
        data,
        sizeof(data),
        100
    );
}

static esp_err_t ssd1306_write_data(
    ssd1306_t *lcd,
    const uint8_t *data,
    size_t len
)
{
    uint8_t *buf = malloc(len + 1);

    if (!buf)
        return ESP_ERR_NO_MEM;

    buf[0] = SSD1306_CONTROL_DATA;

    memcpy(&buf[1], data, len);

    esp_err_t err = i2c_master_transmit(
        lcd->dev,
        buf,
        len + 1,
        1000
    );

    free(buf);

    return err;
}

esp_err_t ssd1306_init(
    ssd1306_t *lcd,
    i2c_master_bus_handle_t bus,
    uint8_t address
)
{
    if (!lcd)
        return ESP_ERR_INVALID_ARG;

    memset(lcd, 0, sizeof(ssd1306_t));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(
            bus,
            &dev_cfg,
            &lcd->dev
        )
    );

    lcd->address = address;

    vTaskDelay(pdMS_TO_TICKS(100));

    // Init SSD1306
    ssd1306_write_cmd(lcd, 0xAE);
    ssd1306_write_cmd(lcd, 0x20);
    ssd1306_write_cmd(lcd, 0x00);
    ssd1306_write_cmd(lcd, 0x81);
    ssd1306_write_cmd(lcd, 0xCF);
    ssd1306_write_cmd(lcd, 0xA1);
    ssd1306_write_cmd(lcd, 0xC8);
    ssd1306_write_cmd(lcd, 0xA6);
    ssd1306_write_cmd(lcd, 0xA8);
    ssd1306_write_cmd(lcd, 0x3F);
    ssd1306_write_cmd(lcd, 0xD3);
    ssd1306_write_cmd(lcd, 0x00);
    ssd1306_write_cmd(lcd, 0xD5);
    ssd1306_write_cmd(lcd, 0x80);
    ssd1306_write_cmd(lcd, 0x8D);
    ssd1306_write_cmd(lcd, 0x14);
    ssd1306_write_cmd(lcd, 0xAF);

    ssd1306_clear(lcd);
    ssd1306_update(lcd);

    lcd->initialized = true;

    ESP_LOGI(TAG, "SSD1306 initialisé à 0x%02X", address);

    return ESP_OK;
}

void ssd1306_clear(ssd1306_t *lcd)
{
    memset(lcd->buffer, 0, sizeof(lcd->buffer));
}

void ssd1306_draw_pixel(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    bool color
)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
        return;

    uint16_t index = x + (y / 8) * SSD1306_WIDTH;

    if (color)
        lcd->buffer[index] |= (1 << (y % 8));
    else
        lcd->buffer[index] &= ~(1 << (y % 8));
}

static void ssd1306_draw_char(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    char c
)
{
    const uint8_t *bitmap = font5x7[(uint8_t)c];

    for (int col = 0; col < 5; col++) {

        uint8_t line = bitmap[col];

        for (int row = 0; row < 7; row++) {

            bool pixel = line & (1 << row);

            ssd1306_draw_pixel(
                lcd,
                x + col,
                y + row,
                pixel
            );
        }
    }
}

void ssd1306_draw_string(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *str
)
{
    while (*str) {

        ssd1306_draw_char(
            lcd,
            x,
            y,
            *str++
        );

        x += 6;
    }
}

esp_err_t ssd1306_update(ssd1306_t *lcd)
{
    ssd1306_write_cmd(lcd, 0x21);
    ssd1306_write_cmd(lcd, 0);
    ssd1306_write_cmd(lcd, 127);

    ssd1306_write_cmd(lcd, 0x22);
    ssd1306_write_cmd(lcd, 0);
    ssd1306_write_cmd(lcd, 7);

    return ssd1306_write_data(
        lcd,
        lcd->buffer,
        sizeof(lcd->buffer)
    );
}
