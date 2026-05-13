#include "ssd1306.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ssd1306_font.h"

static const char *TAG = "SSD1306";

#define SSD1306_CONTROL_CMD 0x00
#define SSD1306_CONTROL_DATA 0x40

/* -------------------------------------------------------------------------- */
/*  SAFE WRITE CMD                                                            */
/* -------------------------------------------------------------------------- */

static esp_err_t ssd1306_write_cmd(ssd1306_t *lcd, uint8_t cmd)
{
    if (!lcd || !lcd->dev)
        return ESP_ERR_INVALID_STATE;

    uint8_t data[2] = {
        SSD1306_CONTROL_CMD,
        cmd};

    return i2c_master_transmit(
        lcd->dev,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100));
}

/* -------------------------------------------------------------------------- */
/*  SAFE WRITE DATA                                                          */
/* -------------------------------------------------------------------------- */

static esp_err_t ssd1306_write_data(
    ssd1306_t *lcd,
    const uint8_t *data,
    size_t len)
{
    if (!lcd || !lcd->dev || !data)
        return ESP_ERR_INVALID_STATE;

    uint8_t buf[SSD1306_WIDTH + 1];

    if (len > SSD1306_WIDTH)
        len = SSD1306_WIDTH;

    buf[0] = SSD1306_CONTROL_DATA;
    memcpy(&buf[1], data, len);

    return i2c_master_transmit(
        lcd->dev,
        buf,
        len + 1,
        pdMS_TO_TICKS(200));
}

/* -------------------------------------------------------------------------- */
/*  INIT                                                                     */
/* -------------------------------------------------------------------------- */

esp_err_t ssd1306_init(
    ssd1306_t *lcd,
    i2c_master_bus_handle_t bus,
    uint8_t address)
{
    ESP_LOGI(TAG, "init");

    if (!lcd || !bus)
        return ESP_ERR_INVALID_ARG;

    return ssd1306_reinit(lcd, bus, address);
}

/* -------------------------------------------------------------------------- */
/*  CLEAR                                                                    */
/* -------------------------------------------------------------------------- */

void ssd1306_clear(ssd1306_t *lcd)
{
    if (!lcd)
        return;

    memset(lcd->buffer, 0, sizeof(lcd->buffer));
}

/* -------------------------------------------------------------------------- */
/*  PIXEL                                                                    */
/* -------------------------------------------------------------------------- */

void ssd1306_draw_pixel(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    bool color)
{
    if (!lcd)
        return;

    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
        return;

    uint16_t index = x + (y / 8) * SSD1306_WIDTH;

    if (color)
        lcd->buffer[index] |= (1 << (y % 8));
    else
        lcd->buffer[index] &= ~(1 << (y % 8));
}

/* -------------------------------------------------------------------------- */
/*  CHAR                                                                     */
/* -------------------------------------------------------------------------- */

static void ssd1306_draw_char(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    char c)
{
    if (!lcd)
        return;

    if ((uint8_t)c < 32 || (uint8_t)c > 126)
        c = '?';

    const uint8_t *bitmap = font5x7[(uint8_t)c];

    for (int col = 0; col < 5; col++)
    {

        uint8_t line = bitmap[col];

        for (int row = 0; row < 7; row++)
        {

            bool pixel = line & (1 << row);

            ssd1306_draw_pixel(
                lcd,
                x + col,
                y + row,
                pixel);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  STRING                                                                   */
/* -------------------------------------------------------------------------- */

void ssd1306_draw_string(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *str)
{
    if (!lcd || !str)
        return;

    while (*str)
    {

        ssd1306_draw_char(lcd, x, y, *str++);

        x += 6;

        if (x >= SSD1306_WIDTH)
            break;
    }
}

/* -------------------------------------------------------------------------- */
/*  UPDATE                                                                   */
/* -------------------------------------------------------------------------- */

esp_err_t ssd1306_update(ssd1306_t *lcd)
{
    if (!lcd || !lcd->dev)
        return ESP_ERR_INVALID_STATE;

    ssd1306_write_cmd(lcd, 0x21);
    ssd1306_write_cmd(lcd, 0);
    ssd1306_write_cmd(lcd, 127);

    ssd1306_write_cmd(lcd, 0x22);
    ssd1306_write_cmd(lcd, 0);
    ssd1306_write_cmd(lcd, 7);

    return ssd1306_write_data(
        lcd,
        lcd->buffer,
        sizeof(lcd->buffer));
}

/* -------------------------------------------------------------------------- */
/*  DEINIT                                                                   */
/* -------------------------------------------------------------------------- */

esp_err_t ssd1306_deinit(ssd1306_t *lcd)
{
    if (!lcd)
        return ESP_ERR_INVALID_ARG;

    if (lcd->dev)
    {

        /* OFF display avant suppression */
        ssd1306_write_cmd(lcd, 0xAE);

        esp_err_t err = i2c_master_bus_rm_device(lcd->dev);

        if (err != ESP_OK)
        {
            ESP_LOGW(TAG,
                     "rm_device failed: %s",
                     esp_err_to_name(err));
        }

        lcd->dev = NULL;
    }

    lcd->initialized = false;

    ESP_LOGI(TAG, "deinit OK");

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  REINIT                                                                   */
/* -------------------------------------------------------------------------- */

esp_err_t ssd1306_reinit(
    ssd1306_t *lcd,
    i2c_master_bus_handle_t bus,
    uint8_t address)
{
    if (!lcd || !bus)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "reinit @0x%02X", address);

    ssd1306_deinit(lcd);

    memset(lcd, 0, sizeof(ssd1306_t));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(
        bus,
        &dev_cfg,
        &lcd->dev);

    if (err != ESP_OK)
        return err;

    lcd->address = address;

    vTaskDelay(pdMS_TO_TICKS(100));

    /* INIT SSD1306 */
    const uint8_t init_cmds[] = {
        0xAE,
        0x20, 0x00,
        0x81, 0xCF,
        0xA1,
        0xC8,
        0xA6,
        0xA8, 0x3F,
        0xD3, 0x00,
        0xD5, 0x80,
        0x8D, 0x14,
        0xAF};

    for (int i = 0; i < sizeof(init_cmds); i++)
    {
        ssd1306_write_cmd(lcd, init_cmds[i]);
    }

    lcd->initialized = true;

    ESP_LOGI(TAG, "reinit OK");

    return ESP_OK;
}

esp_err_t ssd1306_write_line(
    ssd1306_t *lcd,
    uint8_t line,
    const char *text)
{
    if (!lcd || !lcd->initialized || !text)
        return ESP_ERR_INVALID_ARG;

    if (line >= SSD1306_PAGES)
        return ESP_ERR_INVALID_ARG;

    uint8_t y = line * 8;

    // clear line (important pour UI propre)
    for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            ssd1306_draw_pixel(lcd, x, y + i, false);
        }
    }

    // draw text
    ssd1306_draw_string(lcd, 0, y, text);

    return ESP_OK;
}

#include <stdarg.h>
#include <stdio.h>

esp_err_t ssd1306_printf(
    ssd1306_t *lcd,
    uint8_t x,
    uint8_t y,
    const char *fmt,
    ...)
{
    if (!lcd || !lcd->initialized || !fmt)
        return ESP_ERR_INVALID_ARG;

    char buf[128];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ssd1306_draw_string(lcd, x, y, buf);

    return ESP_OK;
}

esp_err_t ssd1306_set_invert(
    ssd1306_t *lcd,
    bool invert)
{
    if (!lcd || !lcd->dev)
        return ESP_ERR_INVALID_STATE;

    return ssd1306_write_cmd(
        lcd,
        invert ? 0xA7 : 0xA6);
}

esp_err_t ssd1306_reset_display(ssd1306_t *lcd)
{
    if (!lcd || !lcd->dev)
        return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;

    err |= ssd1306_write_cmd(lcd, 0xAE); // display OFF
    err |= ssd1306_write_cmd(lcd, 0x20); // horizontal addressing mode
    err |= ssd1306_write_cmd(lcd, 0x00);
    err |= ssd1306_write_cmd(lcd, 0x81);
    err |= ssd1306_write_cmd(lcd, 0xCF);
    err |= ssd1306_write_cmd(lcd, 0xA6); // normal display
    err |= ssd1306_write_cmd(lcd, 0xAF); // display ON

    ssd1306_clear(lcd);
    ssd1306_update(lcd);

    return err;
}
