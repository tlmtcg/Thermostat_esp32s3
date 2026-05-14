#include "oled_service.h"

#include "ssd1306.h"
#include "sht31.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include "time_utils.h"
#include "font3x5.h"
#include "font5x7.h"

static const char *TAG = "OLED_SERVICE";

/* -------------------------------------------------------------------------- */
/*  GLOBAL OLED                                                               */
/* -------------------------------------------------------------------------- */

ssd1306_t oled;

/* -------------------------------------------------------------------------- */
/*  OLED TASK                                                                 */
/* -------------------------------------------------------------------------- */

static void oled_task(void *arg)
{
    char line[32];

    while (1)
    {
        /*
         * IMPORTANT:
         * effacer framebuffer
         */

        ssd1306_clear(&oled);

        /*
         * titre
         */

        char hhmmss[16];
        time_utils_get_hour_str(hhmmss, sizeof(hhmmss));
        ssd1306_draw_string(&oled, 0, 0, hhmmss);

        /*
         * lecture SHT31
         */

        const sht31_state_t *state = sht31_get_state();

        if (state->valid)
        {
            snprintf(
                line,
                sizeof(line),
                "TEMPERATURE : %.1f C",
                state->temperature);

            ssd1306_draw_string(
                &oled,
                0,
                16,
                line);

            snprintf(
                line,
                sizeof(line),
                "HUMIDITE    : %.1f %%",
                state->humidity);

            ssd1306_draw_string(
                &oled,
                0,
                32,
                line);
        }
        else
        {
            ssd1306_draw_string(
                &oled,
                0,
                16,
                "SHT31 ERROR");
        }

        /*
         * UPDATE DISPLAY
         */

        esp_err_t err = ssd1306_update(&oled);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG,
                     "update failed: %s",
                     esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------------------------------------------------------------------------- */
/*  INIT                                                                      */
/* -------------------------------------------------------------------------- */

esp_err_t oled_service_init(
    i2c_master_bus_handle_t bus)
{
    ESP_LOGI(TAG, "OLED init");

    esp_err_t err =
        ssd1306_init(
            &oled,
            bus,
            0x3C);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "ssd1306 init failed");
        return err;
    }

    /*
     * écran boot
     */

    ssd1306_clear(&oled);

    ssd1306_draw_string(
        &oled,
        0,
        0,
        "THERMOSTAT");

    ssd1306_draw_string(
        &oled,
        0,
        16,
        "Starting...");

    ssd1306_update(&oled);

    /*
     * start task
     */

    xTaskCreate(
        oled_task,
        "oled_task",
        4096,
        NULL,
        5,
        NULL);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  SHOW BOOT                                                                 */
/* -------------------------------------------------------------------------- */

void oled_service_show_boot(void)
{
    ssd1306_clear(&oled);

    ssd1306_draw_string(
        &oled,
        0,
        0,
        "THERMOSTAT");

    ssd1306_draw_string(
        &oled,
        0,
        16,
        "Starting...");

    ssd1306_update(&oled);
}

/* -------------------------------------------------------------------------- */
/*  SHOW ERROR                                                                */
/* -------------------------------------------------------------------------- */

void oled_service_show_error(
    const char *msg)
{
    ssd1306_clear(&oled);

    ssd1306_draw_string(
        &oled,
        0,
        0,
        "ERROR");

    if (msg)
    {
        ssd1306_draw_string(
            &oled,
            0,
            16,
            msg);
    }

    ssd1306_update(&oled);
}

/* -------------------------------------------------------------------------- */
/*  SHOW TEXT                                                                 */
/* -------------------------------------------------------------------------- */

void oled_service_show_text(
    const char *line1,
    const char *line2,
    const char *line3)
{
    ssd1306_clear(&oled);

    if (line1)
    {
        ssd1306_draw_string(
            &oled,
            0,
            0,
            line1);
    }

    if (line2)
    {
        ssd1306_draw_string(
            &oled,
            0,
            16,
            line2);
    }

    if (line3)
    {
        ssd1306_draw_string(
            &oled,
            0,
            32,
            line3);
    }

    ssd1306_update(&oled);
}

/* -------------------------------------------------------------------------- */
/*  SHOW TEMP + HUM                                                           */
/* -------------------------------------------------------------------------- */

void oled_service_show_temp_hum(
    float temp,
    float hum)
{
    char line1[32];
    char line2[32];

    snprintf(
        line1,
        sizeof(line1),
        "Temp: %.1f C",
        temp);

    snprintf(
        line2,
        sizeof(line2),
        "Hum : %.1f %%",
        hum);

    ssd1306_clear(&oled);

    ssd1306_draw_string(
        &oled,
        0,
        0,
        "THERMOSTAT");

    ssd1306_draw_string(
        &oled,
        0,
        16,
        line1);

    ssd1306_draw_string(
        &oled,
        0,
        32,
        line2);

    ssd1306_update(&oled);
}

