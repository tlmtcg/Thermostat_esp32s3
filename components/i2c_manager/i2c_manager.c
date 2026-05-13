#include "i2c_manager.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "time_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  LOG TAG                                                                   */
/* -------------------------------------------------------------------------- */

static const char *TAG = "I2C_MANAGER";

/* -------------------------------------------------------------------------- */
/*  GLOBAL I2C BUS                                                            */
/* -------------------------------------------------------------------------- */

// Handle du bus I2C global
i2c_master_bus_handle_t i2c_bus = NULL;
// Mutex pour protéger l'accès concurrent au bus I2C
SemaphoreHandle_t i2c_mutex = NULL;

/* -------------------------------------------------------------------------- */
/*  SCAN JSON                                                                 */
/* -------------------------------------------------------------------------- */

// Stockage du résultat du scan au format JSON
static cJSON *i2c_devices_json = NULL;

/* -------------------------------------------------------------------------- */
/*  SCAN RESULT                                                               */
/* -------------------------------------------------------------------------- */

// Structure pour stocker les résultats du scan I2C
static i2c_scan_result_t scan_result = {0};

/* -------------------------------------------------------------------------- */
/*  FREE JSON                                                                 */
/* -------------------------------------------------------------------------- */

// Libère la mémoire allouée pour le JSON
static void free_json(void)
{
    if (i2c_devices_json) {
        cJSON_Delete(i2c_devices_json);
        i2c_devices_json = NULL;
    }
}

/* -------------------------------------------------------------------------- */
/*  FREE SCAN RESULT                                                          */
/* -------------------------------------------------------------------------- */

// Libère la mémoire allouée pour les résultats du scan
static void free_scan(void)
{
    if (scan_result.addresses) {
        free(scan_result.addresses);
        scan_result.addresses = NULL;
    }

    scan_result.count = 0;
    memset(scan_result.timestamp, 0, sizeof(scan_result.timestamp));
}

/* -------------------------------------------------------------------------- */
/*  INIT BUS                                                                  */
/* -------------------------------------------------------------------------- */

// Initialise le bus I2C avec les broches SDA, SCL et la fréquence spécifiée
esp_err_t i2c_manager_init(int sda, int scl, int freq)
{
    // Vérifie si le bus est déjà initialisé
    if (i2c_bus != NULL) {
        ESP_LOGW(TAG, "I2C déjà initialisé");
        return ESP_OK;
    }

    // Crée un mutex pour protéger l'accès au bus I2C
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE(TAG, "Échec de la création du mutex I2C");
        return ESP_ERR_NO_MEM;
    }

    // Configuration du bus I2C
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, // Ignore les glitches de 7 cycles d'horloge
        .flags.enable_internal_pullup = true, // Active les pull-ups internes
    };

    ESP_LOGI(TAG, "Init I2C SDA=%d SCL=%d Freq=%d", sda, scl, freq);

    // Initialise le bus I2C
    esp_err_t err = i2c_new_master_bus(&cfg, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init I2C: %s", esp_err_to_name(err));
        // Nettoie le mutex en cas d'erreur
        vSemaphoreDelete(i2c_mutex);
        i2c_mutex = NULL;
        return err;
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  DELETE BUS                                                                */
/* -------------------------------------------------------------------------- */

// Supprime le bus I2C et libère les ressources associées
esp_err_t i2c_manager_deinit(void)
{
    if (!i2c_bus) {
        ESP_LOGW(TAG, "Bus I2C déjà supprimé");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Suppression bus I2C...");

    // Supprime le bus I2C
    esp_err_t err = i2c_del_master_bus(i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur suppression bus: %s", esp_err_to_name(err));
        return err;
    }

    i2c_bus = NULL;

    // Supprime le mutex
    if (i2c_mutex != NULL) {
        vSemaphoreDelete(i2c_mutex);
        i2c_mutex = NULL;
    }

    ESP_LOGI(TAG, "Bus I2C supprimé");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  SAFE DEVICE PROBE                                                         */
/* -------------------------------------------------------------------------- */

// Vérifie si un appareil I2C existe à l'adresse spécifiée
bool i2c_device_exists(i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (!bus) {
        return false;
    }

    // Verrouille le mutex pour éviter les accès concurrents
    I2C_LOCK();
    esp_err_t err = i2c_master_probe(bus, addr, 100); // Timeout de 100 ms
    I2C_UNLOCK(); // Déverrouille le mutex

    return err == ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  SCAN FULL I2C BUS                                                         */
/* -------------------------------------------------------------------------- */

// Effectue un scan complet du bus I2C pour détecter les appareils connectés
esp_err_t i2c_manager_scan(void)
{
    if (!i2c_bus) {
        ESP_LOGE(TAG, "Bus I2C non initialisé");
        return ESP_ERR_INVALID_STATE;
    }

    // Nettoie les anciens résultats de scan
    free_json();
    free_scan();

    // Alloue de la mémoire pour stocker les adresses détectées (max 128 adresses possibles)
    scan_result.addresses = malloc(128);
    if (!scan_result.addresses) {
        return ESP_ERR_NO_MEM;
    }

    // Récupère l'horodatage du scan
    time_utils_get_time_str(
        scan_result.timestamp,
        sizeof(scan_result.timestamp)
    );

    ESP_LOGI(TAG, "Début scan I2C...");

    // Verrouille le mutex pour éviter les accès concurrents pendant le scan
    I2C_LOCK();
    // Scan des adresses I2C standard (0x08 à 0x77)
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t err = i2c_master_probe(i2c_bus, addr, 100); // Timeout de 100 ms
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device trouvé: 0x%02X", addr);
            scan_result.addresses[scan_result.count++] = addr;
        }
    }
    I2C_UNLOCK(); // Déverrouille le mutex

    ESP_LOGI(TAG, "Scan terminé (%d devices)", (int)scan_result.count);

    // Construit le JSON des appareils détectés
    i2c_devices_json = cJSON_CreateObject();
    if (!i2c_devices_json) {
        return ESP_ERR_NO_MEM;
    }

    // Ajoute l'horodatage au JSON
    cJSON_AddStringToObject(i2c_devices_json, "timestamp", scan_result.timestamp);

    // Crée un tableau JSON pour les appareils
    cJSON *arr = cJSON_AddArrayToObject(i2c_devices_json, "devices");
    if (!arr) {
        return ESP_ERR_NO_MEM;
    }

    // Ajoute chaque adresse détectée au tableau JSON
    for (size_t i = 0; i < scan_result.count; i++) {
        cJSON *obj = cJSON_CreateObject();
        if (!obj) {
            continue;
        }
        cJSON_AddNumberToObject(obj, "address", scan_result.addresses[i]);
        cJSON_AddItemToArray(arr, obj);
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  JSON GETTER                                                               */
/* -------------------------------------------------------------------------- */

// Retourne le JSON des appareils détectés lors du dernier scan
cJSON *i2c_manager_get_devices_json(void)
{
    return i2c_devices_json;
}

/* -------------------------------------------------------------------------- */
/*  RAW RESULT GETTER                                                         */
/* -------------------------------------------------------------------------- */

// Retourne les résultats bruts du dernier scan I2C
const i2c_scan_result_t *i2c_manager_get_scan_result(void)
{
    return &scan_result;
}
