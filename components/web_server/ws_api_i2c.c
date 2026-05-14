#include "esp_http_server.h"

#include "ws_api_i2c.h"

#include "i2c_manager.h"
#include "config_storage.h"

#include "sht31.h"
#include "ssd1306.h"

#include "esp_log.h"

#include "cJSON.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  LOG TAG                                                                   */
/* -------------------------------------------------------------------------- */

static const char *TAG = "WS_API_I2C";

#define CONFIG_FILE "/sdcard/config.json"

/* -------------------------------------------------------------------------- */
/*  I2C CONFIG STRUCT                                                         */
/* -------------------------------------------------------------------------- */

typedef struct
{

    int sda_gpio;

    int scl_gpio;

    int freq_hz;

} i2c_config_t;

/* -------------------------------------------------------------------------- */
/*  CURRENT CONFIG                                                            */
/* -------------------------------------------------------------------------- */

static i2c_config_t current_config = {

    .sda_gpio = CONFIG_I2C_MANAGER_SDA,

    .scl_gpio = CONFIG_I2C_MANAGER_SCL,

    .freq_hz = CONFIG_I2C_MANAGER_FREQ};

/* -------------------------------------------------------------------------- */
/*  SCAN I2C                                                                  */
/* -------------------------------------------------------------------------- */

static esp_err_t i2c_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,
             "Scan I2C demandé");

    esp_err_t ret = i2c_manager_scan();

    if (ret != ESP_OK)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Échec scan I2C");
    }

    cJSON *devices_json =
        i2c_manager_get_devices_json();

    if (!devices_json)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Aucun résultat");
    }

    char *json_str =
        cJSON_Print(devices_json);

    if (!json_str)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Erreur JSON");
    }

    httpd_resp_set_type(
        req,
        "application/json");

    esp_err_t resp =
        httpd_resp_send(
            req,
            json_str,
            HTTPD_RESP_USE_STRLEN);

    free(json_str);

    return resp;
}

/* -------------------------------------------------------------------------- */
/*  GET LAST SCAN                                                             */
/* -------------------------------------------------------------------------- */

static esp_err_t i2c_devices_handler(httpd_req_t *req)
{
    cJSON *devices_json =
        i2c_manager_get_devices_json();

    if (!devices_json)
    {

        const char *empty_json =
            "{\"status\":\"no_scan_yet\",\"devices\":[]}";

        httpd_resp_set_type(
            req,
            "application/json");

        return httpd_resp_send(
            req,
            empty_json,
            HTTPD_RESP_USE_STRLEN);
    }

    char *json_str =
        cJSON_Print(devices_json);

    if (!json_str)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Erreur JSON");
    }

    httpd_resp_set_type(
        req,
        "application/json");

    esp_err_t ret =
        httpd_resp_send(
            req,
            json_str,
            HTTPD_RESP_USE_STRLEN);

    free(json_str);

    return ret;
}

/* -------------------------------------------------------------------------- */
/*  GET CONFIG                                                                */
/* -------------------------------------------------------------------------- */

static esp_err_t i2c_config_get_handler(httpd_req_t *req)
{
    char json[256];

    snprintf(
        json,
        sizeof(json),
        "{\"sda\":%d,\"scl\":%d,\"freq\":%d}",
        current_config.sda_gpio,
        current_config.scl_gpio,
        current_config.freq_hz);

    httpd_resp_set_type(
        req,
        "application/json");

    return httpd_resp_send(
        req,
        json,
        HTTPD_RESP_USE_STRLEN);
}

/* -------------------------------------------------------------------------- */
/*  POST CONFIG                                                               */
/* -------------------------------------------------------------------------- */

static esp_err_t i2c_config_post_handler(httpd_req_t *req)
{
    int total = req->content_len;

    if (total <= 0 || total > 1024)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Payload invalide");
    }

    /* ---------------------------------------------------------------------- */
    /*  READ BODY                                                             */
    /* ---------------------------------------------------------------------- */

    char *buf = malloc(total + 1);

    if (!buf)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "OOM");
    }

    int received = 0;

    while (received < total)
    {

        int r = httpd_req_recv(
            req,
            buf + received,
            total - received);

        if (r <= 0)
        {

            free(buf);

            return httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Lecture incomplète");
        }

        received += r;
    }

    buf[received] = '\0';

    ESP_LOGI(TAG,
             "POST body = %s",
             buf);

    /* ---------------------------------------------------------------------- */
    /*  PARSE JSON                                                            */
    /* ---------------------------------------------------------------------- */

    cJSON *root = cJSON_Parse(buf);

    free(buf);

    if (!root)
    {

        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "JSON invalide");
    }

    cJSON *sda_json =
        cJSON_GetObjectItem(root, "sda");

    cJSON *scl_json =
        cJSON_GetObjectItem(root, "scl");

    cJSON *freq_json =
        cJSON_GetObjectItem(root, "freq");

    if (sda_json)
        current_config.sda_gpio =
            sda_json->valueint;

    if (scl_json)
        current_config.scl_gpio =
            scl_json->valueint;

    if (freq_json)
        current_config.freq_hz =
            freq_json->valueint;

    cJSON_Delete(root);

    /* ---------------------------------------------------------------------- */
    /*  SAVE CONFIG                                                           */
    /* ---------------------------------------------------------------------- */

    i2c_save_config_to_sdcard();

    /* ---------------------------------------------------------------------- */
    /*  STOP DEVICES                                                          */
    /* ---------------------------------------------------------------------- */

    ESP_LOGI(TAG,
             "Suppression devices I2C...");

    /*
     * IMPORTANT:
     * Tous les devices doivent être supprimés
     * AVANT la suppression du bus.
     */

    sht31_deinit();
    ssd1306_deinit(&oled);

    /* ---------------------------------------------------------------------- */
    /*  DELETE BUS                                                            */
    /* ---------------------------------------------------------------------- */

    ESP_LOGI(TAG,
             "Suppression bus I2C...");

    esp_err_t err =
        i2c_manager_deinit();

    if (err != ESP_OK)
    {

        ESP_LOGE(TAG,
                 "Erreur suppression bus: %s",
                 esp_err_to_name(err));

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Suppression bus échouée");
    }

    /* ---------------------------------------------------------------------- */
    /*  RECREATE BUS                                                          */
    /* ---------------------------------------------------------------------- */

    ESP_LOGI(TAG,
             "Réinitialisation bus I2C...");

    err = i2c_manager_init(
        current_config.sda_gpio,
        current_config.scl_gpio,
        current_config.freq_hz);

    if (err != ESP_OK)
    {

        ESP_LOGE(TAG,
                 "Erreur init bus: %s",
                 esp_err_to_name(err));

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Init bus échouée");
    }

    /* ---------------------------------------------------------------------- */
    /*  RESTART DEVICES                                                       */
    /* ---------------------------------------------------------------------- */

    ESP_LOGI(TAG,
             "Redémarrage SHT31...");

    err = sht31_start(
        i2c_manager_get_bus(),
        0x44);

    if (err != ESP_OK)
    {

        ESP_LOGE(TAG,
                 "Erreur restart SHT31: %s",
                 esp_err_to_name(err));

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Restart SHT31 échoué");
    }

    /* ---------------------------------------------------------------------- */
    /*  SUCCESS                                                               */
    /* ---------------------------------------------------------------------- */

    httpd_resp_set_type(
        req,
        "application/json");

    return httpd_resp_sendstr(
        req,
        "{\"status\":\"ok\"}");
}

/* -------------------------------------------------------------------------- */
/*  LOAD CONFIG                                                               */
/* -------------------------------------------------------------------------- */

static void i2c_load_config_from_sdcard(void)
{
    cJSON *config_json =
        load_json_from_sdcard(CONFIG_FILE);

    if (!config_json)
    {

        ESP_LOGW(TAG,
                 "Aucune config I2C trouvée");

        return;
    }

    cJSON *i2c_json =
        cJSON_GetObjectItem(config_json,
                            "i2c");

    if (!i2c_json)
    {

        cJSON_Delete(config_json);

        return;
    }

    cJSON *sda_json =
        cJSON_GetObjectItem(i2c_json,
                            "sda");

    cJSON *scl_json =
        cJSON_GetObjectItem(i2c_json,
                            "scl");

    cJSON *freq_json =
        cJSON_GetObjectItem(i2c_json,
                            "freq");

    if (sda_json)
        current_config.sda_gpio =
            sda_json->valueint;

    if (scl_json)
        current_config.scl_gpio =
            scl_json->valueint;

    if (freq_json)
        current_config.freq_hz =
            freq_json->valueint;

    cJSON_Delete(config_json);

    ESP_LOGI(TAG,
             "Config I2C chargée SDA=%d SCL=%d FREQ=%d",
             current_config.sda_gpio,
             current_config.scl_gpio,
             current_config.freq_hz);
}

/* -------------------------------------------------------------------------- */
/*  SAVE CONFIG                                                               */
/* -------------------------------------------------------------------------- */

void i2c_save_config_to_sdcard(void)
{
    cJSON *config_json =
        load_json_from_sdcard(CONFIG_FILE);

    if (!config_json)
    {

        config_json =
            cJSON_CreateObject();

        if (!config_json)
        {

            ESP_LOGE(TAG,
                     "Erreur création JSON");

            return;
        }
    }

    cJSON *i2c_json =
        cJSON_GetObjectItem(config_json,
                            "i2c");

    if (!i2c_json)
    {

        i2c_json =
            cJSON_AddObjectToObject(
                config_json,
                "i2c");
    }

    cJSON_ReplaceItemInObject(
        i2c_json,
        "sda",
        cJSON_CreateNumber(
            current_config.sda_gpio));

    cJSON_ReplaceItemInObject(
        i2c_json,
        "scl",
        cJSON_CreateNumber(
            current_config.scl_gpio));

    cJSON_ReplaceItemInObject(
        i2c_json,
        "freq",
        cJSON_CreateNumber(
            current_config.freq_hz));

    char *json_str =
        cJSON_Print(config_json);

    if (json_str)
    {

        FILE *file =
            fopen(CONFIG_FILE, "w");

        if (file)
        {

            fwrite(
                json_str,
                1,
                strlen(json_str),
                file);

            fclose(file);

            ESP_LOGI(TAG,
                     "Configuration I2C sauvegardée");
        }

        free(json_str);
    }

    cJSON_Delete(config_json);
}

/* -------------------------------------------------------------------------- */
/*  REGISTER API                                                              */
/* -------------------------------------------------------------------------- */

esp_err_t ws_register_i2c_api(httpd_handle_t server)
{
    i2c_load_config_from_sdcard();

    /* ---------------- SCAN ---------------- */

    httpd_uri_t uri_scan = {
        .uri = "/api/i2c/scan",
        .method = HTTP_POST,
        .handler = i2c_scan_handler,
        .user_ctx = NULL};

    httpd_register_uri_handler(
        server,
        &uri_scan);

    /* ---------------- DEVICES ---------------- */

    httpd_uri_t uri_devices = {
        .uri = "/api/i2c/devices",
        .method = HTTP_GET,
        .handler = i2c_devices_handler,
        .user_ctx = NULL};

    httpd_register_uri_handler(
        server,
        &uri_devices);

    /* ---------------- CONFIG GET ---------------- */

    httpd_uri_t uri_config_get = {
        .uri = "/api/i2c/config",
        .method = HTTP_GET,
        .handler = i2c_config_get_handler,
        .user_ctx = NULL};

    httpd_register_uri_handler(
        server,
        &uri_config_get);

    /* ---------------- CONFIG POST ---------------- */

    httpd_uri_t uri_config_post = {
        .uri = "/api/i2c/config/set",
        .method = HTTP_POST,
        .handler = i2c_config_post_handler,
        .user_ctx = NULL};

    httpd_register_uri_handler(
        server,
        &uri_config_post);

    ESP_LOGI(TAG,
             "API I2C enregistrée");

    return ESP_OK;
}
