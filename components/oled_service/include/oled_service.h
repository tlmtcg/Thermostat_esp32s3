#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "ssd1306.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* -------------------------------------------------------------------------- */
    /*  GLOBAL OLED                                                               */
    /* -------------------------------------------------------------------------- */

    extern ssd1306_t oled;

    /* -------------------------------------------------------------------------- */
    /*  INIT SERVICE                                                               */
    /* -------------------------------------------------------------------------- */

    esp_err_t oled_service_init(
        i2c_master_bus_handle_t bus);

    /* -------------------------------------------------------------------------- */
    /*  DISPLAY HELPERS                                                           */
    /* -------------------------------------------------------------------------- */

    void oled_service_show_boot(void);

    void oled_service_show_error(
        const char *msg);

    void oled_service_show_text(
        const char *line1,
        const char *line2,
        const char *line3);

    void oled_service_show_temp_hum(
        float temp,
        float hum);

#ifdef __cplusplus
}
#endif
