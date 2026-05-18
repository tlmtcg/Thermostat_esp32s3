#include "heating_program.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdint.h>
#include <time.h>
#include "time_utils.h"

static const char *TAG = "HEATING_PRG";
#define NVS_NAMESPACE "storage"
#define NVS_KEY "heat_v5" // Nouvelle clé pour éviter les conflits de taille

chauffage_config_t config;

// Définit un point de consigne (calcul auto des secondes)
void heating_set_point(chauffage_config_t *conf, jour_t j, int index, int h, int m, int s, float temp)
{
    if (j < NB_JOURS && index < NB_PLAGES)
    {
        conf->planning[j][index].secondes_minuit = (h * 3600) + (m * 60) + s;
        conf->planning[j][index].temperature = temp;
    }
}

// Retourne la température selon les secondes écoulées depuis minuit
float heating_get_temp(const chauffage_config_t *conf, jour_t j, uint32_t now_sec)
{
    if (j >= NB_JOURS)
        return -1.0f;

    // Valeur par défaut : on prend la dernière plage du jour (souvent la nuit)
    float temp_cible = conf->planning[j][NB_PLAGES - 1].temperature;
    uint32_t dernier_seuil = 0;

    for (int i = 0; i < NB_PLAGES; i++)
    {
        uint32_t seuil = conf->planning[j][i].secondes_minuit;
        // On cherche la plage passée la plus proche de l'heure actuelle
        if (now_sec >= seuil && seuil >= dernier_seuil)
        {
            dernier_seuil = seuil;
            temp_cible = conf->planning[j][i].temperature;
        }
    }
    return temp_cible;
}

esp_err_t heating_init(chauffage_config_t *conf) 
{
    nvs_handle_t h;
    esp_err_t err;

    // 1. Initialisation de securite en RAM (17 degres partout)
    memset(conf, 0, sizeof(chauffage_config_t));
    for(int d = 0; d < 7; d++) {
        for(int p = 0; p < 4; p++) {
            conf->planning[d][p].temperature = 17.0f;
            conf->planning[d][p].secondes_minuit = p * 14400; // Espacement de 4h
        }
    }

    // 2. Ouverture de la NVS (Namespace "storage")
    err = nvs_open("storage", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ouverture NVS (%s)", esp_err_to_name(err));
        return err;
    }

    // 3. Lecture du Blob
    size_t required_size = sizeof(chauffage_config_t);
    err = nvs_get_blob(h, NVS_KEY, conf, &required_size);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Planning charge avec succes (%u octets).", (unsigned int)required_size);
    } 
    else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS vide. Sauvegarde des valeurs par defaut...");
        // On enregistre les 17 degres par defaut pour le prochain boot
        err = nvs_set_blob(h, NVS_KEY, conf, sizeof(chauffage_config_t));
        if (err == ESP_OK) {
            nvs_commit(h);
        }
    } 
    else {
        ESP_LOGE(TAG, "Erreur lecture NVS : %s", esp_err_to_name(err));
    }

    nvs_close(h);
    return err;
}

esp_err_t heating_save(const chauffage_config_t *conf) 
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, NVS_KEY, conf, sizeof(chauffage_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(h);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Sauvegarde reussie (Taille: %u)", sizeof(chauffage_config_t));
        }
    }
    
    nvs_close(h);
    return err;
}

// Fonction pour appliquer les valeurs par défaut en RAM
void heating_reset_defaults(chauffage_config_t *conf) {
    float temps[] = {21.0f, 20.0f, 21.0f, 17.0f};
    uint32_t heures[] = {6, 12, 18, 22}; // 6h, 12h, 18h, 22h

    for (int d = 0; d < NB_JOURS; d++) {
        for (int p = 0; p < 4; p++) {
            conf->planning[d][p].temperature = temps[p];
            conf->planning[d][p].secondes_minuit = heures[p] * 3600;
        }
    }
    ESP_LOGI(TAG, "Planning réinitialisé aux valeurs par défaut.");
}

char* heating_get_json(const chauffage_config_t *conf) {
    cJSON *root = cJSON_CreateObject();
    cJSON *days_arr = cJSON_CreateArray();

    for (int d = 0; d < 7; d++) {
        cJSON *day_obj = cJSON_CreateObject();
        cJSON *slots_arr = cJSON_CreateArray();

        for (int p = 0; p < 4; p++) {
            cJSON *slot = cJSON_CreateObject();
            cJSON_AddNumberToObject(slot, "id", p);
            cJSON_AddNumberToObject(slot, "time", conf->planning[d][p].secondes_minuit);
            cJSON_AddNumberToObject(slot, "temp", conf->planning[d][p].temperature);
            cJSON_AddItemToArray(slots_arr, slot);
        }

        cJSON_AddNumberToObject(day_obj, "day_idx", d);
        cJSON_AddItemToObject(day_obj, "slots", slots_arr);
        cJSON_AddItemToArray(days_arr, day_obj);
    }

    cJSON_AddItemToObject(root, "planning", days_arr);

    // Génération de la chaîne (Format compact pour le web)
    char *json_string = cJSON_PrintUnformatted(root);
    
    // Nettoyage de la mémoire cJSON
    cJSON_Delete(root);

    return json_string; // /!\ N'oublie pas de free() ce pointeur après usage
}

/**
 * @brief Retourne la température cible basée sur l'heure locale actuelle de l'ESP32.
 * 
 * @param conf Pointeur vers la configuration du chauffage.
 * @return float La température cible, ou -1.0f en cas d'erreur.
 */
float heating_get_temp_current(const chauffage_config_t *conf)
{
    if (conf == NULL) {
        return -1.0f;
    }

    // 1. Récupération de l'heure locale via le composant time_utils
    struct tm local_time = time_utils_get_local_time();

    // 2. Conversion du jour (struct tm : 0 = Dimanche, 1 = Lundi, etc.)
    // ATTENTION : Ajustez si votre énumération jour_t commence par Lundi (0) au lieu de Dimanche (0)
    jour_t j = (jour_t)local_time.tm_wday; 

    // Validation du jour
    if (j >= NB_JOURS) {
        return -1.0f;
    }

    // 3. Calcul des secondes écoulées depuis minuit
    uint32_t now_sec = (uint32_t)((local_time.tm_hour * 3600) + 
                                  (local_time.tm_min * 60) + 
                                  local_time.tm_sec);

    // 4. Algorithme de recherche de la plage horaire (identique à votre fonction initiale)
    float temp_cible = conf->planning[j][NB_PLAGES - 1].temperature;
    uint32_t dernier_seuil = 0;

    for (int i = 0; i < NB_PLAGES; i++)
    {
        uint32_t seuil = conf->planning[j][i].secondes_minuit;
        
        // Recherche de la plage passée la plus proche de l'heure actuelle
        if (now_sec >= seuil && seuil >= dernier_seuil)
        {
            dernier_seuil = seuil;
            temp_cible = conf->planning[j][i].temperature;
        }
    }

    return temp_cible;
}

const chauffage_config_t *heating_get_config(void){
    return &config;
}