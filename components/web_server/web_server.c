#include "web_server.h"
#include "esp_log.h"
#include "esp_err.h"

#include "ws_static.h"
#include "ws_api_wifi.h"
#include "ws_api_time.h"
#include "ws_api_led.h"
#include "ws_api_weather.h"
#include "ws_api_alarms.h"
#include "ws_api_freebox.h"
#include "ws_api_logs.h"
#include "ws_api_sys.h"
#include "ws_api_program.h"
#include "ws_api_jeedom.h"
#include "ws_api_relay.h"
#include "ws_api_sd.h"
#include "ws_api_task.h"
#include "ws_api_i2c.h"

static const char *TAG = "WEB_SERVER";

static httpd_handle_t server = NULL;

 int g_http_handlers_used = 0;
 int g_http_handlers_max = 128;

/**
 * @brief Enregistre toutes les routes d’un module et log en cas d’erreur.
 */
esp_err_t register_module(httpd_handle_t server,
                          const char *name,
                          esp_err_t (*fn)(httpd_handle_t))
{
    static const char *registered[32];
    static int count = 0;

    for (int i = 0; i < count; i++) {
        if (strcmp(registered[i], name) == 0) {
            ESP_LOGW(TAG, "Module %s déjà enregistré", name);
            return ESP_OK;
        }
    }

    registered[count++] = name;

    return fn(server);
}

/**
 * @brief Initialise et démarre le serveur HTTP.
 */
httpd_handle_t start_webserver(void)
{
    if (server != NULL)
    {
        ESP_LOGW(TAG, "Serveur déjà démarré");
        return server;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_uri_handlers = 128;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Démarrage du serveur HTTP sur le port %d...",
             config.server_port);

    esp_err_t err = httpd_start(&server, &config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erreur httpd_start() : %s",
                 esp_err_to_name(err));

        server = NULL;

        return NULL;
    }

    ESP_LOGI(TAG, "Serveur HTTP démarré, enregistrement des routes...");

    register_module(server, "static", ws_register_static);
    register_module(server, "wifi", ws_register_wifi_api);
    register_module(server, "time", ws_register_time_api);
    register_module(server, "led", ws_register_led_api);
    register_module(server, "weather", ws_register_weather_api);
    register_module(server, "alarms", ws_register_alarms_api);
    register_module(server, "freebox", ws_register_freebox_api);
    register_module(server, "logs", ws_register_logs_api);
    register_module(server, "program", ws_register_program_api);
    register_module(server, "sys", ws_register_sys_api);
    register_module(server, "jeedom", register_jeedom_web_api);
    register_module(server, "relay", ws_register_relay_api);
    register_module(server, "sd", ws_register_sd_api);
    register_module(server, "tasks", ws_register_tasks_api);
    register_module(server, "i2c", ws_register_i2c_api);

    ESP_LOGI(TAG, "Serveur Web prêt.");

    return server;
}

void stop_webserver(void)
{
    if (server)
    {
        ESP_LOGI(TAG, "Arrêt du serveur HTTP");

        httpd_stop(server);

        server = NULL;
    }
}
