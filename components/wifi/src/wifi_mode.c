#include "wifi_mode.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "WIFI_MODE";

void wifi_mode_update(bool sta_connected, int ap_clients, bool test_mode)
{
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    // CONDITION DE FERMETURE : 
    // On ne passe en STA seul QUE SI on a le WiFi ET qu'il n'y a plus PERSONNE sur l'AP
    if (sta_connected && ap_clients == 0 && !test_mode)
    {
        if (current_mode != WIFI_MODE_STA)
        {
            ESP_LOGW(TAG, "Mode Auto : STA ok et AP libre. Fermeture AP.");
            esp_wifi_set_mode(WIFI_MODE_STA);
        }
    }
    // CONDITION D'OUVERTURE :
    // On active l'APSTA si le WiFi est perdu OU si un client veut se connecter
    else 
    {
        if (current_mode != WIFI_MODE_APSTA)
        {
            ESP_LOGI(TAG, "Mode Auto : Activation AP+STA (Secours ou Utilisateur)");
            esp_wifi_set_mode(WIFI_MODE_APSTA);
        }
    }
}
