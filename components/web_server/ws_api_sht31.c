#include "ws_api_sht31.h"
#include "esp_log.h"
#include "sht31.h"
#include <stdarg.h>
#include <stdio.h>
#include "i2c_manager.h"

static esp_err_t sht31_handler(httpd_req_t *req)
{
    const sht31_state_t *st = sht31_get_state();

    char json[256];

    snprintf(json, sizeof(json),
        "{"
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"valid\":%s,"
        "\"last_update\":%lld"
        "}",
        st->temperature,
        st->humidity,
        st->valid ? "true" : "false",
        (long long)st->last_update
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t ws_register_sht31_api(httpd_handle_t server)
{
    httpd_uri_t uri = {
        .uri = "/api/sensors/sht31",
        .method = HTTP_GET,
        .handler = sht31_handler
    };

    httpd_register_uri_handler(server, &uri);

    ESP_LOGI("SHT31_API", "API SHT31 enregistrée");
    return ESP_OK;
}

esp_err_t i2c_manager_reinit(int sda_gpio, int scl_gpio, int freq_hz)
{
    if (i2c_bus) {
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
    }

    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    return i2c_new_master_bus(&cfg, &i2c_bus);
}
