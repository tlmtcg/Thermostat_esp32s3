
#include "ws_registry.h"
#include <esp_log.h>

// --- Inclusions des modules ---
#include "ws_static.h"
#include "ws_api_wifi.h"
#include "ws_api_time.h"
#include "ws_api_led.h"
#include "ws_api_weather.h"
#include "ws_api_alarms.h"
#include "ws_api_logs.h"
#include "ws_api_freebox.h"
#include "ws_api_program.h"
#include "ws_api_sys.h"
#include "ws_api_jeedom.h"
#include "ws_api_relay.h"
#include "ws_api_sd.h"


static const char *TAG = "WS_REGISTRY";

// Flag pour empêcher la saturation des slots en cas de reconnexions WiFi
static bool is_web_registered = false;

// Tableau des fonctions d'enregistrement de chaque module
static ws_register_fn_t registry[] = {
    ws_register_wifi_api,
    ws_register_time_api,
    ws_register_led_api,
    ws_register_weather_api,
    ws_register_alarms_api,
    ws_register_freebox_api,
    ws_register_logs_api,
    ws_register_sys_api,
    ws_register_program_api,
    ws_register_jeedom_api,
    ws_register_relay_api,
    ws_register_sd_api,
    ws_register_static, // Le module statique est souvent le plus gourmand (12+ routes)
};

void ws_register_all(httpd_handle_t server)
{
    if (server == NULL) {
        ESP_LOGE(TAG, "Handle serveur NULL : enregistrement impossible");
        return;
    }

    if (is_web_registered) {
        ESP_LOGW(TAG, "Routes déjà présentes en mémoire. Blocage de la ré-inscription.");
        return;
    }

    int module_count = sizeof(registry) / sizeof(registry[0]);
    ESP_LOGI(TAG, "Lancement de l'enregistrement de %d modules...", module_count);

    for (int i = 0; i < module_count; i++) {
        if (registry[i] != NULL) {
            registry[i](server);
        }
    }

    is_web_registered = true;
    ESP_LOGI(TAG, "==> Architecture Web prête (Vérifiez config.max_uri_handlers si erreur) <==");
}