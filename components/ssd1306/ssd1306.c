#include "ssd1306.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"     
#include "esp_log.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "font5x7.h"

static const char *TAG = "SSD1306";

#define SSD1306_CONTROL_CMD   0x00
#define SSD1306_CONTROL_DATA  0x40

extern ssd1306_t oled; // Liaison avec l'instance globale partagée

/**
 * @brief Envoi d'une commande unique à l'écran (Version robuste d'origine)
 */
esp_err_t ssd1306_write_cmd(ssd1306_t *lcd, uint8_t cmd)
{
    if (!lcd || !lcd->dev) return ESP_ERR_INVALID_STATE;
    
    // Utilisation d'un buffer de transmission local strict pour sécuriser le signal STOP
    uint8_t tx_buf[2] = { SSD1306_CONTROL_CMD, cmd };
    return i2c_master_transmit(lcd->dev, tx_buf, 2, pdMS_TO_TICKS(200));
}

/**
 * @brief Séquence matérielle d'initialisation de l'écran
 */
esp_err_t ssd1306_reinit(ssd1306_t *lcd, i2c_master_bus_handle_t bus, uint8_t address)
{
    if (!lcd || !bus) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Initializing SSD1306 at address 0x%02X...", address);

    if (lcd->dev) {
        i2c_master_bus_rm_device(lcd->dev);
        lcd->dev = NULL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = CONFIG_I2C_MANAGER_FREQ,
    };

    // Attribution de l'identifiant matériel avant toute communication
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &lcd->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(err));
        return err;
    }

    lcd->address = address;
    
    // Pause de stabilisation du bus
    vTaskDelay(pdMS_TO_TICKS(100));

    // Séquence d'initialisation pas-à-pas (Évite le débordement d'interruptions)
    if ((err = ssd1306_write_cmd(lcd, 0xAE)) != ESP_OK) return err; // Display OFF
    if ((err = ssd1306_write_cmd(lcd, 0x20)) != ESP_OK) return err; // Addressing Mode
    if ((err = ssd1306_write_cmd(lcd, 0x00)) != ESP_OK) return err; // Horizontal Mode
    if ((err = ssd1306_write_cmd(lcd, 0x21)) != ESP_OK) return err; // Column Addr
    if ((err = ssd1306_write_cmd(lcd, 0x00)) != ESP_OK) return err; // Start
    if ((err = ssd1306_write_cmd(lcd, 0x7F)) != ESP_OK) return err; // End
    if ((err = ssd1306_write_cmd(lcd, 0x22)) != ESP_OK) return err; // Page Addr
    if ((err = ssd1306_write_cmd(lcd, 0x00)) != ESP_OK) return err; // Start
    if ((err = ssd1306_write_cmd(lcd, 0x07)) != ESP_OK) return err; // End
    if ((err = ssd1306_write_cmd(lcd, 0x81)) != ESP_OK) return err; // Contrast
    if ((err = ssd1306_write_cmd(lcd, 0xCF)) != ESP_OK) return err; 
    if ((err = ssd1306_write_cmd(lcd, 0xA1)) != ESP_OK) return err; // Segment remap
    if ((err = ssd1306_write_cmd(lcd, 0xC8)) != ESP_OK) return err; // Scan direction
    if ((err = ssd1306_write_cmd(lcd, 0xA6)) != ESP_OK) return err; // Normal display
    if ((err = ssd1306_write_cmd(lcd, 0xA8)) != ESP_OK) return err; // Multiplex
    if ((err = ssd1306_write_cmd(lcd, 0x3F)) != ESP_OK) return err; 
    if ((err = ssd1306_write_cmd(lcd, 0xD3)) != ESP_OK) return err; // Offset
    if ((err = ssd1306_write_cmd(lcd, 0x00)) != ESP_OK) return err; 
    if ((err = ssd1306_write_cmd(lcd, 0xD5)) != ESP_OK) return err; // Clock divide
    if ((err = ssd1306_write_cmd(lcd, 0x80)) != ESP_OK) return err; 
    if ((err = ssd1306_write_cmd(lcd, 0x8D)) != ESP_OK) return err; // Charge pump
    if ((err = ssd1306_write_cmd(lcd, 0x14)) != ESP_OK) return err; 
    if ((err = ssd1306_write_cmd(lcd, 0xAF)) != ESP_OK) return err; // Display ON

    lcd->initialized = true;
    
    return ssd1306_update(lcd);
}

esp_err_t ssd1306_init(ssd1306_t *lcd, i2c_master_bus_handle_t bus, uint8_t address)
{
    if (!lcd) return ESP_ERR_INVALID_ARG;
    
    memset(lcd->buffer, 0, sizeof(lcd->buffer));
    lcd->initialized = false;
    lcd->dev = NULL; 
    
    lcd->font = &SSD1306_FONT_5X7;

    return ssd1306_reinit(lcd, bus, address);
}

esp_err_t ssd1306_deinit(ssd1306_t *lcd)
{
    if (!lcd) return ESP_ERR_INVALID_ARG;
    if (lcd->dev) {
        ssd1306_write_cmd(lcd, 0xAE); 
        i2c_master_bus_rm_device(lcd->dev);
        lcd->dev = NULL;
    }
    lcd->initialized = false;
    return ESP_OK;
}

void ssd1306_clear(ssd1306_t *lcd)
{
    if (!lcd) return;
    memset(lcd->buffer, 0, sizeof(lcd->buffer));
}

/**
 * @brief Rafraîchissement stable par page de 129 octets
 */
esp_err_t ssd1306_update(ssd1306_t *lcd)
{
    if (!lcd || !lcd->dev) return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;
    uint8_t page_buf[129]; 
    page_buf[0] = SSD1306_CONTROL_DATA;

    for (uint8_t page = 0; page < 8; page++) 
    {
        if ((err = ssd1306_write_cmd(lcd, 0xB0 + page)) != ESP_OK) return err;
        if ((err = ssd1306_write_cmd(lcd, 0x00)) != ESP_OK) return err;
        if ((err = ssd1306_write_cmd(lcd, 0x10)) != ESP_OK) return err;

        memcpy(&page_buf[1], &lcd->buffer[page * 128], 128);

        err = i2c_master_transmit(lcd->dev, page_buf, 129, pdMS_TO_TICKS(500));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update page %d: %s", page, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

void ssd1306_draw_pixel(ssd1306_t *lcd, uint8_t x, uint8_t y, bool color)
{
    if (!lcd || x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    uint16_t index = x + (y / 8) * SSD1306_WIDTH;
    
    if (color) {
        lcd->buffer[index] |= (1 << (y % 8));
    } else {
        lcd->buffer[index] &= ~(1 << (y % 8));
    }
}

void ssd1306_draw_line(ssd1306_t *lcd, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    if (!lcd) return;
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        ssd1306_draw_pixel(lcd, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

/**
 * @brief Rendu d'un caractère (Correction de la lecture du tableau imbriqué font5x7[256][5])
 */
static void ssd1306_draw_char(ssd1306_t *lcd, uint8_t x, uint8_t y, char c)
{
    if (!lcd || !lcd->font || !lcd->font->bitmap) return;

    uint8_t w = lcd->font->width;   // 5
    uint8_t h = lcd->font->height;  // 7

    // Repositionnement direct sur l'index du tableau [c][colonne] de ton font5x7 d'origine
    const uint8_t *char_bitmap = &lcd->font->bitmap[(uint8_t)c * w];

    for (int col = 0; col < w; col++) {
        uint8_t byte_line = char_bitmap[col];
        for (int row = 0; row < h; row++) {
            ssd1306_draw_pixel(lcd, x + col, y + row, byte_line & (1 << row));
        }
    }
}

void ssd1306_draw_string(ssd1306_t *lcd, uint8_t x, uint8_t y, const char *str)
{
    if (!lcd || !lcd->font || !str) return;

    uint8_t step = lcd->font->width + lcd->font->spacing;

    while (*str && x < SSD1306_WIDTH) {
        ssd1306_draw_char(lcd, x, y, *str++);
        x += step;
    }
}

esp_err_t ssd1306_write_line(ssd1306_t *lcd, uint8_t line, const char *text)
{
    if (!lcd || line >= SSD1306_PAGES) return ESP_ERR_INVALID_ARG;
    
    memset(&lcd->buffer[line * SSD1306_WIDTH], 0, SSD1306_WIDTH);
    ssd1306_draw_string(lcd, 0, line * 8, text);
    return ssd1306_update(lcd);
}

esp_err_t ssd1306_printf(ssd1306_t *lcd, uint8_t x, uint8_t y, const char *fmt, ...)
{
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ssd1306_draw_string(lcd, x, y, buf);
    return ESP_OK;
}

esp_err_t ssd1306_set_invert(ssd1306_t *lcd, bool invert)
{
    return ssd1306_write_cmd(lcd, invert ? 0xA7 : 0xA6);
}

esp_err_t ssd1306_reset_display(ssd1306_t *lcd)
{
    ssd1306_clear(lcd);
    return ssd1306_update(lcd);
}
