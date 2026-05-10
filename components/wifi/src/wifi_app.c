#include "wifi_app.h"
#include "wifi_manager.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "led_ctrl.h"
#include "alert_manager.h"
#include "task_manager.h"

static const char *TAG = "WIFI_APP";

// --- TIMER GLOBAL ---
static TimerHandle_t xReconnectTimer = NULL;

// --- CALLBACKS ---

static void vTimerReconnectCallback(TimerHandle_t xTimer)
{
    int count = wifi_get_ap_client_count();

    if (count > 0)
    {
        // Il y a au moins un client connecté
        ESP_LOGW(TAG, "Client AP détecté (%d). On évite de perturber la radio.", count);
        xTimerStart(xReconnectTimer, 0);
    }
    else
    {
        // Personne n'est connecté, on peut réessayer le mode STA
        ESP_LOGI(TAG, "AP libre. Tentative de reconnexion STA...");
        esp_wifi_connect();
    }
}

static void on_sta_connected(const esp_ip4_addr_t *ip)
{
    alert_remove("Panne wifi");
    ESP_LOGI(TAG, "STA connectée ! IP : " IPSTR, IP2STR(ip));
    // C'est ici qu'on autorise les tâches à travailler
    // task_manager_set_active(BIT_WEATHER_EN | BIT_JEEDOM_EN | BIT_NTP_EN, true);

    if (xReconnectTimer)
        xTimerStop(xReconnectTimer, 0);
    // start_webserver();
}

static void on_sta_failed(int reason)
{
    alert_add("Panne wifi"); // La LED clignote en erreur
    // Sécurité : On coupe les tâches si plus de réseau
    // task_manager_set_active(BIT_WEATHER_EN | BIT_JEEDOM_EN | BIT_NTP_EN, false);
    if (xReconnectTimer)
        xTimerStart(xReconnectTimer, 0);
}

// --- API PUBLIQUE ---

void wifi_app_start(void)
{
    ESP_LOGI(TAG, "Initialisation du module WiFi (ESP-IDF v6)...");

    // 1. Timer de reconnexion
    xReconnectTimer = xTimerCreate("WiFi_Retrier",
                                   pdMS_TO_TICKS(5000),
                                   pdFALSE,
                                   NULL,
                                   vTimerReconnectCallback);

    // 2. Callbacks WiFi Manager
    static wifi_callbacks_t cb = {0};
    cb.on_sta_connected = on_sta_connected;
    cb.on_sta_failed = on_sta_failed;
    cb.on_ap_started = NULL; // si tu veux gérer l’AP, je peux ajouter un callback

    // 3. Lancement du WiFi Manager
    wifi_manager_init(&cb);

    ESP_LOGI(TAG, "wifi_app_start terminé.");
}
