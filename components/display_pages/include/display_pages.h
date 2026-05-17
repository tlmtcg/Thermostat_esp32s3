#pragma once

#include "ssd1306.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PAGE_MAIN = 0,
    PAGE_HISTORY,
    PAGE_WIFI,
    PAGE_TIME,
     PAGE_ALERTS,
    PAGE_COUNT

} ui_page_t;

void display_pages_init(ssd1306_t *lcd);
void display_pages_start(void);

#ifdef __cplusplus
}
#endif
