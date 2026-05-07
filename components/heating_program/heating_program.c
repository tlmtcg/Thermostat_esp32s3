#include "heating_program.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "HEATING_PRG";

// Définit un point de consigne (calcul auto des secondes)
void heating_set_point(chauffage_config_t *conf, jour_t j, int index, int h, int m, int s, float temp) {
    if (j < NB_JOURS && index < NB_PLAGES) {
        conf->planning[j][index].secondes_minuit = (h * 3600) + (m * 60) + s;
        conf->planning[j][index].temperature = temp;
    }
}

// Retourne la température selon les secondes écoulées depuis minuit
float heating_get_temp(const chauffage_config_t *conf, jour_t j, uint32_t now_sec) {
    if (j >= NB_JOURS) return -1.0f;

    // Valeur par défaut : on prend la dernière plage du jour (souvent la nuit)
    float temp_cible = conf->planning[j][NB_PLAGES - 1].temperature;
    uint32_t dernier_seuil = 0;

    for (int i = 0; i < NB_PLAGES; i++) {
        uint32_t seuil = conf->planning[j][i].secondes_minuit;
        // On cherche la plage passée la plus proche de l'heure actuelle
        if (now_sec >= seuil && seuil >= dernier_seuil) {
            dernier_seuil = seuil;
            temp_cible = conf->planning[j][i].temperature;
        }
    }
    return temp_cible;
}

// Sauvegarde bloc complet dans la NVS
esp_err_t heating_save(const chauffage_config_t *conf) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "heat_prec", conf, sizeof(chauffage_config_t));
    if (err == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return err;
}

// Charge ou initialise par défaut
esp_err_t heating_init(chauffage_config_t *conf) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &h);
    size_t size = sizeof(chauffage_config_t);
    
    if (err != ESP_OK || nvs_get_blob(h, "heat_prec", conf, &size) != ESP_OK) {
        for(int d=0; d<NB_JOURS; d++) {
            heating_set_point(conf, d, 0, 7, 30, 0, 20.0f);
            heating_set_point(conf, d, 1, 12, 0, 0, 19.0f);
            heating_set_point(conf, d, 2, 18, 0, 0, 21.0f);
            heating_set_point(conf, d, 3, 22, 0, 0, 17.0f);
        }
        if (err == ESP_OK) nvs_close(h);
        return ESP_ERR_NOT_FOUND;
    }
    nvs_close(h);
    return ESP_OK;
}