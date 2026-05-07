#include "heating_program.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "HEATING_PRG_TEST";

void test_heating_program(void) {
    // Initialisation NVS
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     nvs_flash_erase();
    //     nvs_flash_init();
    // }

    chauffage_config_t config;
    heating_init(&config);

    // Exemple : Lundi à 07:45:30 -> 22.0°C
    heating_set_point(&config, LUNDI, 0, 7, 45, 30, 22.0f);
    heating_save(&config);

    // Lecture de l'heure système
    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);

    uint32_t secs_today = (ti.tm_hour * 3600) + (ti.tm_min * 60) + ti.tm_sec;
    jour_t j = (ti.tm_wday == 0) ? DIMANCHE : (jour_t)(ti.tm_wday - 1);

    // Récupération de la consigne
    float consigne = heating_get_temp(&config, j, secs_today);
    ESP_LOGI(TAG,"Consigne pour %02d:%02d:%02d : %.1f C\n", ti.tm_hour, ti.tm_min, ti.tm_sec, consigne);
}