#include "wifi_events.h"
#include "wifi_manager.h"
#include "wifi_state_machine.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "app_context.h"

static const char *TAG = "WIFI_EVENTS";
static const wifi_callbacks_t *s_cb = NULL;
static int s_ap_clients_count = 0; 

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    // --- 1. GESTION DE L'ACCESS POINT (AP) ---
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "AP démarré");
        wifi_state_update(WIFI_STATE_AP_ONLY);
        if (s_cb && s_cb->on_ap_started)
            s_cb->on_ap_started();
    }

    if (base == WIFI_EVENT && (id == WIFI_EVENT_AP_STACONNECTED || id == WIFI_EVENT_AP_STADISCONNECTED))
    {
        wifi_sta_list_t sta_list;
        // On demande au driver WiFi la liste réelle des clients associés
        esp_wifi_ap_get_sta_list(&sta_list);
        
        // On met à jour notre compteur avec la réalité du hardware
        s_ap_clients_count = sta_list.num;

        wifi_manager_update_client_count(s_ap_clients_count);
        ESP_LOGI(TAG, "Mise à jour AP : %d client(s) actif(s)", s_ap_clients_count);

        // --- ACTION DE FERMETURE ---
        if (s_ap_clients_count == 0 && wifi_state_get() == WIFI_STATE_STA_CONNECTED) {
            ESP_LOGW(TAG, "Plus personne sur l'AP, passage en STA pur.");
            esp_wifi_set_mode(WIFI_MODE_STA);
        }
    }

    // --- 2. GESTION DE LA STATION (STA) ---
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "STA perdue (Raison: %d)", ev->reason);

        wifi_state_update(WIFI_STATE_STA_FAILED);

        if (s_cb && s_cb->on_sta_failed)
        {
            s_cb->on_sta_failed(ev->reason);
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&ev->ip_info.ip));

        wifi_state_update(WIFI_STATE_STA_CONNECTED);

        if (s_cb && s_cb->on_sta_connected)
        {
            s_cb->on_sta_connected(&ev->ip_info.ip);
        }
    }
}

void wifi_events_init(const wifi_callbacks_t *callbacks)
{
    s_cb = callbacks;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
}
