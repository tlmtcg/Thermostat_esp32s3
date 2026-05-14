#pragma once
#include <stdint.h>
#include "ssd1306.h"

// Police 3x5 (3 colonnes × 5 lignes)
// Format : Chaque caractère fait 3 pixels de large sur 5 pixels de haut.
extern const uint8_t font3x5[256][3];

static const ssd1306_font_t SSD1306_FONT_3X5 = {
    .bitmap = (const uint8_t *)font3x5,
    .width = 3,
    .height = 5,
    .spacing = 1
};