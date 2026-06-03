/**
 * @file sht31.c
 * @brief Pilote pour le capteur SHT31 (température/humidité) sur ESP32.
 *        Gère l'initialisation, la lecture, les erreurs, et la récupération.
 *
 * @note Utilise le bus I2C pour communiquer avec le capteur.
 *       Compatible avec ESP-IDF v6.
 */

#include "sht31.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"
#include "alert_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_manager.h"
#include "sdkconfig.h"
#include "thermostat.h"

// =========================================================================
// CONFIGURATION
// =========================================================================

/** @brief Tag pour les logs ESP. */
static const char *TAG = "SHT31";

/** @brief Commande pour une mesure en haute résolution. */
#define SHT31_CMD_MEAS_HIGHREP 0x2400

/** @brief Commande pour un reset logiciel. */
#define SHT31_CMD_SOFT_RESET 0x30A2

/** @brief Adresse I2C par défaut du SHT31. */
#define SHT31_DEFAULT_ADDR 0x44

/** @brief Intervalle de lecture par défaut (en ms). */
#define SHT31_DEFAULT_READ_INTERVAL_MS 5000

/** @brief Nombre d'erreurs consécutives avant le premier log. */
#define SHT31_ERROR_LOG_FIRST_COUNT 3

/** @brief Intervalle de logs pour les erreurs consécutives. */
#define SHT31_ERROR_LOG_EVERY_COUNT 10

// =========================================================================
// STRUCTURES ET VARIABLES GLOBALES
// =========================================================================

/**
 * @brief Contexte global du SHT31.
 *        Contient le bus I2C, le périphérique, la configuration et l'état d'exécution.
 */
typedef struct
{
    i2c_master_bus_handle_t bus;      /**< Pointeur vers le bus I2C. */
    i2c_master_dev_handle_t dev;      /**< Pointeur vers le périphérique I2C. */
    sht31_config_t config;            /**< Configuration du capteur. */
    sht31_runtime_t runtime;          /**< État d'exécution. */
} sht31_ctx_t;

/** @brief Instance globale du contexte SHT31. */
static sht31_ctx_t g_sht31 = {
    .config = {
        .addr = SHT31_DEFAULT_ADDR,
        .read_interval_ms = SHT31_DEFAULT_READ_INTERVAL_MS,
        .log_to_sd = true,
    },
    .runtime = {
        .last_error = "",
    },
};

// =========================================================================
// FONCTIONS STATIQUES (INTERNES)
// =========================================================================

/**
 * @brief Calcule le CRC8 pour les données du SHT31.
 *
 * @param data Pointeur vers les données à vérifier.
 * @param len Longueur des données.
 * @return uint8_t Le CRC8 calculé.
 */
static uint8_t sht31_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }

    return crc;
}

/**
 * @brief Définit le dernier message d'erreur.
 *
 * @param message Message d'erreur à enregistrer.
 */
static void sht31_set_error(const char *message)
{
    if (!message)
        message = "";

    snprintf(
        g_sht31.runtime.last_error,
        sizeof(g_sht31.runtime.last_error),
        "%s",
        message
    );
}

/**
 * @brief Efface les erreurs et réinitialise les compteurs.
 */
static void sht31_clear_error(void)
{
    g_sht31.runtime.last_error[0] = '\0';
    g_sht31.runtime.last_error_code = ESP_OK;
    g_sht31.runtime.consecutive_error_count = 0;
    alert_remove("Erreur capteur SHT31");
    alert_remove("Capteur SHT31 absent");
}

/**
 * @brief Enregistre une erreur et met à jour l'état.
 *
 * @param err Code d'erreur ESP.
 */
static void sht31_record_error(esp_err_t err)
{
    g_sht31.runtime.valid = false;
    g_sht31.runtime.error_count++;
    g_sht31.runtime.consecutive_error_count++;
    g_sht31.runtime.last_error_code = err;
    g_sht31.runtime.last_error_at = time(NULL);
    sht31_set_error(esp_err_to_name(err));

    // Ajouter une alerte uniquement à la première erreur consécutive
    if (err == ESP_ERR_INVALID_STATE)
    {
        alert_add("Capteur SHT31 absent");
    }

    if (g_sht31.runtime.consecutive_error_count == SHT31_ERROR_LOG_FIRST_COUNT)
    {
        alert_add("Erreur capteur SHT31");
    }

    // Logs filtrés pour éviter le spam
    uint32_t consecutive = g_sht31.runtime.consecutive_error_count;
    if (consecutive <= SHT31_ERROR_LOG_FIRST_COUNT ||
        (consecutive % SHT31_ERROR_LOG_EVERY_COUNT) == 0)
    {
        ESP_LOGW(
            TAG,
            "Lecture SHT31 échouée: %s (consecutives=%lu, total=%lu, last_error=%s)",
            esp_err_to_name(err),
            (unsigned long)consecutive,
            (unsigned long)g_sht31.runtime.error_count,
            g_sht31.runtime.last_error
        );
    }

    // Notifier le thermostat que les données ne sont plus valides
    thermostat_update_indoor_data(
        g_sht31.runtime.temperature,
        g_sht31.runtime.humidity,
        false
    );
}

/**
 * @brief Écrit une commande au capteur SHT31.
 *
 * @param cmd Commande à envoyer.
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
static esp_err_t sht31_write_cmd(uint16_t cmd)
{
    if (!g_sht31.dev)
    {
        ESP_LOGE(TAG, "sht31_write_cmd: g_sht31.dev is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF),
    };

    return i2c_master_transmit(
        g_sht31.dev,
        data,
        sizeof(data),
        pdMS_TO_TICKS(200)
    );
}

/**
 * @brief Attache le périphérique SHT31 au bus I2C.
 *
 * @param addr Adresse I2C du capteur.
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
static esp_err_t sht31_attach_device(uint8_t addr)
{
    if (!g_sht31.bus)
    {
        ESP_LOGE(TAG, "sht31_attach_device: g_sht31.bus is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = CONFIG_I2C_MANAGER_FREQ,
    };

    return i2c_master_bus_add_device(g_sht31.bus, &cfg, &g_sht31.dev);
}

// =========================================================================
// FONCTIONS PUBLIQUES
// =========================================================================

/**
 * @brief Initialise le capteur SHT31.
 *
 * @param bus Pointeur vers le bus I2C.
 * @param addr Adresse I2C du capteur.
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (!bus)
    {
        ESP_LOGE(TAG, "sht31_init: bus is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (addr == 0)
    {
        ESP_LOGE(TAG, "sht31_init: addr is 0");
        return ESP_ERR_INVALID_ARG;
    }

    // Si déjà initialisé, vérifier si le bus a changé
    if (g_sht31.runtime.initialized)
    {
        if (g_sht31.bus != bus)
        {
            ESP_LOGW(TAG, "Bus I2C changé, réinitialisation du SHT31");
            sht31_deinit(); // Nettoyer l'ancien état
        }
        else
        {
            ESP_LOGW(TAG, "SHT31 déjà initialisé");
            return ESP_OK;
        }
    }

    g_sht31.bus = bus;
    g_sht31.config.addr = addr;

    esp_err_t err = sht31_attach_device(g_sht31.config.addr);
    if (err != ESP_OK)
    {
        alert_add("Capteur SHT31 absent");
        sht31_record_error(err);
        ESP_LOGE(TAG, "Erreur add device: %s", esp_err_to_name(err));
        return err;
    }

    g_sht31.runtime.initialized = true;
    sht31_clear_error();

    ESP_LOGI(TAG, "SHT31 initialisé @0x%02X", g_sht31.config.addr);
    return ESP_OK;
}

/**
 * @brief Désinitialise le capteur SHT31.
 */
void sht31_deinit(void)
{
    sht31_stop();

    if (g_sht31.dev)
    {
        i2c_master_bus_rm_device(g_sht31.dev);
        g_sht31.dev = NULL;
    }

    g_sht31.bus = NULL;
    g_sht31.runtime.initialized = false;
    g_sht31.runtime.valid = false;

    ESP_LOGI(TAG, "SHT31 désinitialisé");
}

/**
 * @brief Effectue un reset logiciel du capteur.
 *
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_reset(void)
{
    return sht31_write_cmd(SHT31_CMD_SOFT_RESET);
}

/**
 * @brief Tentative de récupération après une erreur.
 *
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_recover(void)
{
    if (!g_sht31.bus)
    {
        ESP_LOGE(TAG, "sht31_recover: g_sht31.bus is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(
        TAG,
        "Tentative de récupération SHT31 après %lu erreur(s) consécutive(s)",
        (unsigned long)g_sht31.runtime.consecutive_error_count
    );

    esp_err_t err = sht31_reset();
    if (err == ESP_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Soft reset SHT31 impossible: %s, réattachement du périphérique", esp_err_to_name(err));

    if (g_sht31.dev)
    {
        i2c_master_bus_rm_device(g_sht31.dev);
        g_sht31.dev = NULL;
    }

    // Vérifier que g_sht31.bus est toujours valide
    if (!g_sht31.bus)
    {
        ESP_LOGE(TAG, "sht31_recover: g_sht31.bus est NULL après réattachement");
        return ESP_ERR_INVALID_STATE;
    }

    err = sht31_attach_device(g_sht31.config.addr);
    if (err != ESP_OK)
    {
        alert_add("Capteur SHT31 absent");
        g_sht31.runtime.initialized = false;
        sht31_record_error(err);
        return err;
    }

    g_sht31.runtime.initialized = true;
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

/**
 * @brief Lit les données de température et d'humidité du capteur.
 *
 * @param temp Pointeur vers la variable de température (en °C).
 * @param hum Pointeur vers la variable d'humidité (en %).
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_read(float *temp, float *hum)
{
    if (!g_sht31.dev)
    {
        ESP_LOGE(TAG, "sht31_read: g_sht31.dev is NULL");
        sht31_record_error(ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }

    if (!temp || !hum)
    {
        ESP_LOGE(TAG, "sht31_read: temp or hum is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t rx[6];
    esp_err_t err = sht31_write_cmd(SHT31_CMD_MEAS_HIGHREP);
    if (err != ESP_OK)
        goto fail;

    vTaskDelay(pdMS_TO_TICKS(25));

    err = i2c_master_receive(
        g_sht31.dev,
        rx,
        sizeof(rx),
        pdMS_TO_TICKS(500)
    );
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_master_receive failed: %s", esp_err_to_name(err));
        goto fail;
    }

    // Vérification des CRC AVANT mise à jour des données
    if (sht31_crc8(&rx[0], 2) != rx[2] || sht31_crc8(&rx[3], 2) != rx[5])
    {
        err = ESP_ERR_INVALID_CRC;
        goto fail;
    }

    // Si tout est OK, mettre à jour les données
    uint16_t raw_t = ((uint16_t)rx[0] << 8) | rx[1];
    uint16_t raw_h = ((uint16_t)rx[3] << 8) | rx[4];

    *temp = -45.0f + (175.0f * ((float)raw_t / 65535.0f));
    *hum = 100.0f * ((float)raw_h / 65535.0f);

    // Mise à jour des données globales UNIQUEMENT si tout est valide
    g_sht31.runtime.temperature = *temp;
    g_sht31.runtime.humidity = *hum;
    g_sht31.runtime.valid = true;
    g_sht31.runtime.last_update = time(NULL);
    g_sht31.runtime.last_success_at = g_sht31.runtime.last_update;
    g_sht31.runtime.read_count++;
    sht31_clear_error();

    // Notifier le thermostat avec les données valides
    thermostat_update_indoor_data(
        g_sht31.runtime.temperature,
        g_sht31.runtime.humidity,
        true
    );
    return ESP_OK;

fail:
    // En cas d'erreur, ne PAS mettre à jour les données globales
    sht31_record_error(err);
    return err;
}

/**
 * @brief Démarre le capteur SHT31.
 *
 * @param bus Pointeur vers le bus I2C.
 * @param addr Adresse I2C du capteur.
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_start(i2c_master_bus_handle_t bus, uint8_t addr)
{
    esp_err_t err = sht31_init(bus, addr);
    if (err != ESP_OK)
        return err;

    g_sht31.runtime.running = true;
    ESP_LOGI(TAG, "SHT31 actif");
    return ESP_OK;
}

/**
 * @brief Arrête le capteur SHT31.
 */
void sht31_stop(void)
{
    g_sht31.runtime.running = false;
    ESP_LOGI(TAG, "SHT31 arrêté");
}

/**
 * @brief Récupère la configuration actuelle.
 *
 * @param out Pointeur vers la structure de configuration à remplir.
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_get_config(sht31_config_t *out)
{
    if (!out)
    {
        ESP_LOGE(TAG, "sht31_get_config: out is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *out = g_sht31.config;
    return ESP_OK;
}

/**
 * @brief Définit la configuration du capteur.
 *
 * @param config Pointeur vers la nouvelle configuration.
 * @return esp_err_t ESP_OK en cas de succès, une erreur sinon.
 */
esp_err_t sht31_set_config(const sht31_config_t *config)
{
    if (!config)
    {
        ESP_LOGE(TAG, "sht31_set_config: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->addr == 0)
    {
        ESP_LOGE(TAG, "sht31_set_config: addr is 0");
        return ESP_ERR_INVALID_ARG;
    }

    sht31_config_t new_config = *config;

    // Appliquer un intervalle par défaut si nécessaire
    if (new_config.read_interval_ms == 0)
    {
        new_config.read_interval_ms = SHT31_DEFAULT_READ_INTERVAL_MS;
    }

    // Si l'adresse a changé et que le capteur est initialisé, réattacher le périphérique
    bool addr_changed = new_config.addr != g_sht31.config.addr;
    if (addr_changed && g_sht31.runtime.initialized)
    {
        if (g_sht31.dev)
        {
            i2c_master_bus_rm_device(g_sht31.dev);
            g_sht31.dev = NULL;
        }

        esp_err_t err = sht31_attach_device(new_config.addr);
        if (err != ESP_OK)
        {
            g_sht31.runtime.initialized = false;
            sht31_record_error(err);
            return err;
        }

        g_sht31.runtime.valid = false;
        sht31_clear_error();
    }

    g_sht31.config = new_config;

    ESP_LOGI(
        TAG,
        "Config appliquée (addr=0x%02X, interval=%u ms, log_to_sd=%d)",
        g_sht31.config.addr,
        (unsigned)g_sht31.config.read_interval_ms,
        g_sht31.config.log_to_sd
    );
    return ESP_OK;
}

/**
 * @brief Récupère l'état d'exécution actuel.
 *
 * @return const sht31_runtime_t* Pointeur vers l'état d'exécution.
 */
const sht31_runtime_t *sht31_get_runtime(void)
{
    return &g_sht31.runtime;
}

/**
 * @brief Définit l'état "running" du capteur.
 *
 * @param running true pour activer, false pour désactiver.
 */
void sht31_set_running(bool running)
{
    g_sht31.runtime.running = running;
}

/**
 * @brief Récupère l'état complet du capteur au format JSON.
 *
 * @return char* Chaîne JSON représentant l'état du capteur.
 *         Doit être libérée avec free() après utilisation.
 */
char *sht31_get_json_status(void)
{
    cJSON *root = cJSON_CreateObject();

    // Ajouter l'état d'exécution
    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddNumberToObject(runtime, "temperature", g_sht31.runtime.temperature);
    cJSON_AddNumberToObject(runtime, "humidity", g_sht31.runtime.humidity);
    cJSON_AddBoolToObject(runtime, "valid", g_sht31.runtime.valid);
    cJSON_AddNumberToObject(runtime, "last_update", (double)g_sht31.runtime.last_update);
    cJSON_AddBoolToObject(runtime, "initialized", g_sht31.runtime.initialized);
    cJSON_AddBoolToObject(runtime, "running", g_sht31.runtime.running);
    cJSON_AddNumberToObject(runtime, "read_count", g_sht31.runtime.read_count);
    cJSON_AddNumberToObject(runtime, "error_count", g_sht31.runtime.error_count);
    cJSON_AddNumberToObject(runtime, "consecutive_error_count", g_sht31.runtime.consecutive_error_count);
    cJSON_AddNumberToObject(runtime, "last_error_code", g_sht31.runtime.last_error_code);
    cJSON_AddNumberToObject(runtime, "last_error_at", (double)g_sht31.runtime.last_error_at);
    cJSON_AddStringToObject(runtime, "last_error", g_sht31.runtime.last_error);

    // Ajouter la configuration
    cJSON *config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddNumberToObject(config, "addr", g_sht31.config.addr);
    cJSON_AddNumberToObject(config, "read_interval_ms", g_sht31.config.read_interval_ms);
    cJSON_AddBoolToObject(config, "log_to_sd", g_sht31.config.log_to_sd);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}
