#include "ws_api_config.h"
#include "config_runtime.h"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "WS_CONFIG";

/* ---------------------------------------------------------
 * GET /api/config
 * --------------------------------------------------------- */
static esp_err_t config_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();

    // --- WEATHER ---
    cJSON *weather = cJSON_CreateObject();
    cJSON_AddStringToObject(weather, "city", g_cfg.weather_city);
    cJSON_AddNumberToObject(weather, "lat", g_cfg.weather_lat);
    cJSON_AddNumberToObject(weather, "lon", g_cfg.weather_lon);
    cJSON_AddItemToObject(root, "weather", weather);

    // --- THERMOSTAT ---
    cJSON *th = cJSON_CreateObject();
    cJSON_AddNumberToObject(th, "offset", g_cfg.thermostat_offset);
    cJSON_AddNumberToObject(th, "hysteresis", g_cfg.thermostat_hysteresis);
    cJSON_AddBoolToObject(th, "auto_mode", g_cfg.thermostat_auto_mode);
    cJSON_AddItemToObject(root, "thermostat", th);

    // --- SHT31 ---
    cJSON *sht = cJSON_CreateObject();
    cJSON_AddNumberToObject(sht, "temp_cal", g_cfg.sht31_temp_calibration);
    cJSON_AddNumberToObject(sht, "hum_cal", g_cfg.sht31_hum_calibration);
    cJSON_AddItemToObject(root, "sht31", sht);

    // --- JEEDOM ---
    cJSON *jee = cJSON_CreateObject();
    cJSON_AddBoolToObject(jee, "enabled", g_cfg.jeedom_enabled);
    cJSON_AddNumberToObject(jee, "id", g_cfg.jeedom_id);
    cJSON_AddItemToObject(root, "jeedom", jee);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(root);

    return ESP_OK;
}

/* ---------------------------------------------------------
 * POST /api/config
 * --------------------------------------------------------- */
static esp_err_t config_post_handler(httpd_req_t *req)
{
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
        return ESP_FAIL;

    buf[len] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root)
        return ESP_FAIL;

    // --- WEATHER ---
    cJSON *weather = cJSON_GetObjectItem(root, "weather");
    if (weather)
    {
        cJSON *city = cJSON_GetObjectItem(weather, "city");
        cJSON *lat = cJSON_GetObjectItem(weather, "lat");
        cJSON *lon = cJSON_GetObjectItem(weather, "lon");

        if (cJSON_IsString(city))
            strncpy(g_cfg.weather_city, city->valuestring, sizeof(g_cfg.weather_city));

        if (cJSON_IsNumber(lat))
            g_cfg.weather_lat = lat->valuedouble;

        if (cJSON_IsNumber(lon))
            g_cfg.weather_lon = lon->valuedouble;
    }

    // --- THERMOSTAT ---
    cJSON *th = cJSON_GetObjectItem(root, "thermostat");
    if (th)
    {
        cJSON *off = cJSON_GetObjectItem(th, "offset");
        cJSON *hys = cJSON_GetObjectItem(th, "hysteresis");
        cJSON *aut = cJSON_GetObjectItem(th, "auto_mode");

        if (cJSON_IsNumber(off))
            g_cfg.thermostat_offset = off->valuedouble;

        if (cJSON_IsNumber(hys))
            g_cfg.thermostat_hysteresis = hys->valuedouble;

        if (cJSON_IsBool(aut))
            g_cfg.thermostat_auto_mode = aut->valueint;
    }

    // --- SHT31 ---
    cJSON *sht = cJSON_GetObjectItem(root, "sht31");
    if (sht)
    {
        cJSON *tc = cJSON_GetObjectItem(sht, "temp_cal");
        cJSON *hc = cJSON_GetObjectItem(sht, "hum_cal");

        if (cJSON_IsNumber(tc))
            g_cfg.sht31_temp_calibration = tc->valuedouble;

        if (cJSON_IsNumber(hc))
            g_cfg.sht31_hum_calibration = hc->valuedouble;
    }

    // --- JEEDOM ---
    cJSON *jee = cJSON_GetObjectItem(root, "jeedom");
    if (jee)
    {
        cJSON *en = cJSON_GetObjectItem(jee, "enabled");
        cJSON *id = cJSON_GetObjectItem(jee, "id");

        if (cJSON_IsBool(en))
            g_cfg.jeedom_enabled = en->valueint;

        if (cJSON_IsNumber(id))
            g_cfg.jeedom_id = id->valueint;
    }

    cJSON_Delete(root);

    // Sauvegarde NVS
    config_runtime_save();

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

/* ---------------------------------------------------------
 * Enregistrement
 * --------------------------------------------------------- */
esp_err_t ws_register_config_api(httpd_handle_t server)
{
    httpd_uri_t uri_get = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = config_get_handler,
        .user_ctx = NULL};

    httpd_uri_t uri_post = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = config_post_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET)", uri_get.uri);
    httpd_register_uri_handler(server, &uri_get);

    ESP_LOGI(TAG, "Register: %s (POST)", uri_post.uri);
    return httpd_register_uri_handler(server, &uri_post);
}
