#include "i2c_manager.h"  // Inclut le header où i2c_scan_result_t est déclaré
#include "esp_log.h"
#include "sdkconfig.h"
#include "time_utils.h"  // Pour le timestamp

static const char *TAG = "I2C_MANAGER";

// Variable globale pour le bus I2C
i2c_master_bus_handle_t i2c_bus_handle = NULL;

// Variable globale pour stocker le JSON des périphériques I2C trouvés
static cJSON *i2c_devices_json = NULL;

// Variable globale pour stocker le résultat du scan
static i2c_scan_result_t scan_result = {0};

// Libère le JSON actuel
static void i2c_manager_free_json(void) {
    if (i2c_devices_json != NULL) {
        cJSON_Delete(i2c_devices_json);
        i2c_devices_json = NULL;
    }
}

// Libère la mémoire allouée pour le résultat du scan
static void i2c_manager_free_scan_result(void) {
    if (scan_result.addresses != NULL) {
        free(scan_result.addresses);
        scan_result.addresses = NULL;
        scan_result.count = 0;
    }
}

// Retourne le JSON des périphériques I2C (pour l'API)
cJSON *i2c_manager_get_devices_json(void) {
    return i2c_devices_json;
}

// Retourne le résultat brut du scan
const i2c_scan_result_t *i2c_manager_get_scan_result(void) {
    return &scan_result;
}

esp_err_t i2c_manager_init(void) {
    if (i2c_bus_handle != NULL) {
        ESP_LOGW(TAG, "I2C déjà initialisé");
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_I2C_MANAGER_SDA,
        .scl_io_num = CONFIG_I2C_MANAGER_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Init I2C SDA=%d SCL=%d Freq=%d",
             CONFIG_I2C_MANAGER_SDA,
             CONFIG_I2C_MANAGER_SCL,
             CONFIG_I2C_MANAGER_FREQ);

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init I2C: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// Fonction pour scanner les adresses I2C et stocker le résultat en JSON
esp_err_t i2c_manager_scan(void) {
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus I2C non initialisé. Appelez i2c_manager_init() d'abord.");
        return ESP_ERR_INVALID_STATE;
    }

    // Libérer les anciens résultats
    i2c_manager_free_json();
    i2c_manager_free_scan_result();

    // ESP_LOGI(TAG, "Début du scan I2C...");

    // Allouer de la mémoire pour stocker les adresses (max 128 périphériques)
    scan_result.addresses = malloc(128 * sizeof(uint8_t));
    if (scan_result.addresses == NULL) {
        ESP_LOGE(TAG, "Échec d'allocation mémoire.");
        return ESP_ERR_NO_MEM;
    }
    scan_result.count = 0;

    // Récupérer le timestamp actuel avec ton composant
    time_utils_get_time_str(scan_result.timestamp, sizeof(scan_result.timestamp));
    // ESP_LOGI(TAG, "Timestamp du scan: %s", scan_result.timestamp);

    // Scanner les adresses I2C de 0x08 à 0x77
    for (uint8_t address = 0x08; address <= 0x77; address++) {
        i2c_master_dev_handle_t device_handle;
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = CONFIG_I2C_MANAGER_FREQ,
        };

        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &device_handle);
        if (ret == ESP_OK) {
            scan_result.addresses[scan_result.count++] = address;
            // ESP_LOGI(TAG, "Périphérique trouvé à l'adresse: 0x%02X", address);
            i2c_master_bus_rm_device(device_handle);
        }
    }

    // Générer le JSON avec cJSON
    i2c_devices_json = cJSON_CreateObject();
    if (i2c_devices_json == NULL) {
        ESP_LOGE(TAG, "Échec de la création de l'objet JSON.");
        i2c_manager_free_scan_result();
        return ESP_ERR_NO_MEM;
    }

    // Ajouter le timestamp et le statut
    cJSON_AddStringToObject(i2c_devices_json, "timestamp", scan_result.timestamp);
    cJSON_AddStringToObject(i2c_devices_json, "status",
                           (scan_result.count > 0) ? "success" : "no_devices_found");

    // Ajouter le tableau des adresses
    cJSON *devices_array = cJSON_AddArrayToObject(i2c_devices_json, "devices");
    if (devices_array == NULL) {
        ESP_LOGE(TAG, "Échec de la création du tableau JSON.");
        i2c_manager_free_json();
        i2c_manager_free_scan_result();
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < scan_result.count; i++) {
        cJSON *device_obj = cJSON_CreateObject();
        if (device_obj == NULL) {
            continue;  // Ignorer si échec
        }
        cJSON_AddNumberToObject(device_obj, "address", scan_result.addresses[i]);
        cJSON_AddItemToArray(devices_array, device_obj);
    }

    if (scan_result.count == 0) {
        ESP_LOGW(TAG, "Aucun périphérique I2C trouvé.");
        return ESP_ERR_NOT_FOUND;
    }

    // ESP_LOGI(TAG, "Scan terminé. %d périphérique(s) trouvé(s).", scan_result.count);
    return ESP_OK;
}

/**
 * @brief Réinitialise le bus I2C avec de nouveaux paramètres (GPIO et fréquence).
 *        Doit être appelée après une modification de la configuration.
 *
 * @param sda_gpio Nouveau GPIO pour SDA.
 * @param scl_gpio Nouveau GPIO pour SCL.
 * @param freq_hz  Nouvelle fréquence en Hz.
 * @return esp_err_t ESP_OK en cas de succès, sinon une erreur.
 */
esp_err_t i2c_manager_reinit(int sda_gpio, int scl_gpio, int freq_hz) {
    // Vérifier que les GPIO sont valides (0-40 pour ESP32-S3)
    if (sda_gpio < 0 || sda_gpio > 40 || scl_gpio < 0 || scl_gpio > 40) {
        ESP_LOGE(TAG, "GPIO invalide: SDA=%d, SCL=%d", sda_gpio, scl_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    // Vérifier que la fréquence est valide (entre 10kHz et 5MHz pour ESP32-S3)
    if (freq_hz < 10000 || freq_hz > 5000000) {
        ESP_LOGE(TAG, "Fréquence I2C invalide: %d Hz", freq_hz);
        return ESP_ERR_INVALID_ARG;
    }

    // Libérer l'ancien bus I2C s'il existe
    if (i2c_bus_handle != NULL) {
        ESP_LOGI(TAG, "Suppression de l'ancien bus I2C...");

        // Supprimer tous les périphériques du bus (si nécessaire)
        // Note: ESP-IDF ne fournit pas de fonction directe pour lister/supprimer tous les périphériques.
        // Si tu as des périphériques ajoutés manuellement, tu devras les supprimer un par un.
        // Ici, on se contente de supprimer le bus.
        esp_err_t ret = i2c_del_master_bus(i2c_bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Échec de la suppression du bus I2C: %s", esp_err_to_name(ret));
            return ret;
        }
        i2c_bus_handle = NULL;
    }

    // Configurer le nouveau bus I2C
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,                // Port I2C (0 ou 1 pour ESP32-S3)
        .sda_io_num = sda_gpio,               // Nouveau GPIO SDA
        .scl_io_num = scl_gpio,               // Nouveau GPIO SCL
        .clk_source = I2C_CLK_SRC_DEFAULT,    // Source d'horloge par défaut
        .glitch_ignore_cnt = 7,               // Ignorer les glitches (par défaut)
        .flags.enable_internal_pullup = true, // Activer les pull-ups internes
    };

    ESP_LOGI(TAG, "Initialisation du nouveau bus I2C: SDA=%d, SCL=%d, Freq=%d Hz",
             sda_gpio, scl_gpio, freq_hz);

    // Créer le nouveau bus I2C
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec de l'initialisation du bus I2C: %s", esp_err_to_name(ret));
        i2c_bus_handle = NULL; // Réinitialiser le handle
        return ret;
    }

    // Mettre à jour la fréquence globale (si tu l'utilises ailleurs)
    // Note: La fréquence est définie par périphérique, pas par bus.
    // Si tu veux appliquer une fréquence par défaut, tu peux la stocker dans une variable globale.
    // Exemple: current_i2c_freq = freq_hz;

    ESP_LOGI(TAG, "Bus I2C réinitialisé avec succès.");
    return ESP_OK;
}
