#include "wifi_mode.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "WIFI_MODE";

void wifi_mode_update(bool sta_connected, int ap_clients, bool test_mode)
{
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    if (sta_connected && ap_clients == 0 && !test_mode)
    {
        if (current_mode != WIFI_MODE_STA)
        {
            ESP_LOGI(TAG, "Passage en STA pur");
            esp_wifi_set_mode(WIFI_MODE_STA);
        }
    }
    else
    {
        if (current_mode != WIFI_MODE_APSTA)
        {
            ESP_LOGW(TAG, "Passage en AP+STA");
            esp_wifi_set_mode(WIFI_MODE_APSTA);
        }
    }
}
