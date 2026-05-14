#pragma once
#include <stdint.h>
#include "ssd1306.h"

// 16 colonnes, chaque colonne fait 3 octets (24 bits) de haut.
extern const uint8_t font16x24[][48] ;

static const ssd1306_font_t SSD1306_FONT_16X24 = {
    .bitmap = (const uint8_t *)font16x24,
    .width = 16,
    .height = 24,
    .spacing = 1
};

