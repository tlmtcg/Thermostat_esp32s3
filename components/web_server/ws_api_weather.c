#include "ws_api_weather.h"
#include "weather_store.h"
#include "esp_log.h"
#include "cJSON.h"
#include <math.h>
#include <config_runtime.h>

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

static esp_err_t api_weather_geocode(httpd_req_t *req)
{
    char city[64];
    if (httpd_req_get_url_query_str(req, city, sizeof(city)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing city");

    char name[64];
    if (httpd_query_key_value(city, "city", name, sizeof(name)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing city");

    double lat, lon;
    if (weather_geocode_city(name, &lat, &lon) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "City not found");

    char json[128];
    snprintf(json, sizeof(json),
             "{\"city\":\"%s\",\"lat\":%.5f,\"lon\":%.5f}",
             name, lat, lon);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t api_weather_update(httpd_req_t *req)
{
    // Lancer la mise à jour météo dans une tâche séparée
    xTaskCreate(weather_update_task, "weather_update_task", 8192, NULL, 5, NULL);

    // Construction de la réponse JSON détaillée
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "update_started");

    // Ajouter les infos météo actuelles de la config
    cJSON *w = cJSON_CreateObject();
    cJSON_AddStringToObject(w, "city", g_cfg.weather_city);
    cJSON_AddNumberToObject(w, "lat", g_cfg.weather_lat);
    cJSON_AddNumberToObject(w, "lon", g_cfg.weather_lon);
    cJSON_AddItemToObject(root, "weather", w);

    // Convertir en texte
    char *out = cJSON_PrintUnformatted(root);

    // Envoyer la réponse
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);

    // Nettoyage
    free(out);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t ws_register_weather_api(httpd_handle_t server)
{
    esp_err_t err;

    /* =======================
     *  GET /api/weather
     * ======================= */
    httpd_uri_t uri_weather_get = {
        .uri = "/api/weather",
        .method = HTTP_GET,
        .handler = meteo_api_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET weather)", uri_weather_get.uri);

    err = httpd_register_uri_handler(server, &uri_weather_get);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GET /api/weather failed: %s", esp_err_to_name(err));
        return err;
    }

    /* =======================
     *  GET /api/weather/geocode
     * ======================= */
    httpd_uri_t uri_geocode = {
        .uri = "/api/weather/geocode",
        .method = HTTP_GET,
        .handler = api_weather_geocode,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET geocode)", uri_geocode.uri);

    err = httpd_register_uri_handler(server, &uri_geocode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GET /api/weather/geocode failed: %s", esp_err_to_name(err));
        return err;
    }

    /* =======================
     *  POST /api/weather/update
     *  (mise à jour manuelle)
     * ======================= */
    httpd_uri_t uri_weather_update = {
        .uri = "/api/weather/update",
        .method = HTTP_POST,
        .handler = api_weather_update,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST update)", uri_weather_update.uri);

    err = httpd_register_uri_handler(server, &uri_weather_update);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "POST /api/weather/update failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "API Weather enregistrée (GET + geocode + update)");

    return ESP_OK;
}
