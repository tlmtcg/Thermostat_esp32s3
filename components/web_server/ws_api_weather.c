#include "ws_api_weather.h"
#include "weather_store.h"
#include "esp_log.h"
#include "cJSON.h"
#include <math.h>

static const char *TAG = "WS_API_WEATHER";

static esp_err_t meteo_api_handler(httpd_req_t *req)
{
    char *json_buf = malloc(4096);
    if (!json_buf)
        return ESP_ERR_NO_MEM;

    weather_data_t data;
    weather_store_get_all(&data); // 🔥 SOURCE UNIQUE

    const char *d_now = get_weather_description(data.current.weather_code);

    int offset = snprintf(json_buf, 4096,
                          "{\"now\":{\"temp\":%.1f,\"hum\":%.0f,\"desc\":\"%s\",\"time\":%ld,\"jee_temp\":%.1f},\"f48_temps\":[",
                          data.current.temperature,
                          data.current.humidity,
                          d_now,
                          data.current.timestamp,
                          data.current.jee_temp);

    for (int i = 0; i < 48; i++)
    {
        offset += snprintf(json_buf + offset, 4096 - offset, "%.1f%s",
                           data.forecast_48h_temp[i],
                           (i < 47) ? "," : "");
    }

    offset += snprintf(json_buf + offset, 4096 - offset, "],\"f48_hums\":[");

    for (int i = 0; i < 48; i++)
    {
        offset += snprintf(json_buf + offset, 4096 - offset, "%.0f%s",
                           data.forecast_48h_hum[i],
                           (i < 47) ? "," : "");
    }

    offset += snprintf(json_buf + offset, 4096 - offset, "],\"f7j\":[");

    for (int i = 0; i < 7; i++)
    {
        const char *desc = get_weather_description(data.forecast_7j[i].weather_code);

        offset += snprintf(json_buf + offset, 4096 - offset,
                           "{\"temp\":%.1f,\"desc\":\"%s\",\"time\":%ld}%s",
                           data.forecast_7j[i].temperature,
                           desc,
                           data.forecast_7j[i].timestamp,
                           (i < 6) ? "," : "");
    }

    snprintf(json_buf + offset, 4096 - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, json_buf, strlen(json_buf));

    free(json_buf);
    return res;
}

esp_err_t ws_register_weather_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_WEATHER: START REGISTER ===");

    esp_err_t err;

    httpd_uri_t meteo_data_uri = {
        .uri = "/api/weather",
        .method = HTTP_GET,
        .handler = meteo_api_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET weather)", meteo_data_uri.uri);

    err = httpd_register_uri_handler(server, &meteo_data_uri);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GET /api/weather failed: %s", esp_err_to_name(err));

        return err;
    }

    ESP_LOGI(TAG, "GET /api/weather -> OK");

    // ESP_LOGI(TAG, "=== WS_API_WEATHER: END REGISTER ===");

    ESP_LOGI(TAG, "API Weather enregistrée (store thread-safe)");

    return ESP_OK;
}
