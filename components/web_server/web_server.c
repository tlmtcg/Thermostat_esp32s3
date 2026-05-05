#include "web_server.h"
#include "esp_log.h"
#include "esp_err.h"

#include "ws_static.h"
#include "ws_api_wifi.h"
#include "ws_api_time.h"
#include "ws_api_led.h"
#include "ws_api_weather.h"
#include "ws_api_alarms.h"
// #include "ws_logs.h"
// #include "ws_api_freebox.h"
// #include "ws_api_sys.h"

static const char *TAG = "WEB_SERVER";

/**
 * @brief Enregistre toutes les routes d’un module et log en cas d’erreur.
 */
static void register_module(httpd_handle_t server,
                            const char *name,
                            esp_err_t (*fn)(httpd_handle_t))
{
    esp_err_t err = fn(server);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Échec enregistrement module %s : %s", name, esp_err_to_name(err));
    else
        ESP_LOGI(TAG, "Module %s OK", name);
}

/**
 * @brief Initialise et démarre le serveur HTTP.
 */
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 32;

    ESP_LOGI(TAG, "Démarrage du serveur HTTP sur le port %d...", config.server_port);

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erreur httpd_start() : %s", esp_err_to_name(err));
        return NULL;
    }

    ESP_LOGI(TAG, "Serveur HTTP démarré, enregistrement des routes...");

    // --- Enregistrement modulaire (SOLID) ---
    register_module(server, "static", ws_register_static);
    register_module(server, "wifi", ws_register_wifi_api);
    register_module(server, "time", ws_register_time_api);
    register_module(server, "led", ws_register_led_api);
    register_module(server, "weather", ws_register_weather_api);
    register_module(server, "alarms", ws_register_alarms_api);
    // register_module(server, "logs",     ws_register_logs_api);
    // register_module(server, "freebox",  ws_register_freebox_api);
    // register_module(server, "sys",      ws_register_sys_api);

    ESP_LOGI(TAG, "Serveur Web prêt.");
    return server;
}
