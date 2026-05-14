#pragma once
#include <stdint.h>
#include "ssd1306.h"

// Police 8x16 (8 colonnes × 16 lignes)
// Format : chaque caractère est représenté par 16 octets (2 octets par ligne, 8 lignes)
extern const uint8_t font8x16[256][16];

static const ssd1306_font_t SSD1306_FONT_8X16 = {
    .bitmap = (const uint8_t *)font8x16,
    .width = 8,
    .height = 16,
    .spacing = 1
};
