/**
 * @file led_db.c
 * @brief Base LED en memoire uniquement.
 */

#include "led_db.h"

#include <string.h>

#include "alert_manager.h"
#include "esp_log.h"

static const char *TAG = "LED_DB";

#define LED_DB_MAX_ITEMS 16

static stored_info_t info_db[LED_DB_MAX_ITEMS];
static int info_count = 0;
static stored_alarm_t alarm_db[LED_DB_MAX_ITEMS];
static int alarm_count = 0;

static led_color_t led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    led_color_t color = {r, g, b};
    return color;
}

static void led_db_register_defaults(void)
{
    // Ambiances de test / reference
    led_db_add_info("Normal", led_rgb(0, 32, 0));
    led_db_add_info("Chauffe", led_rgb(48, 16, 0));
    led_db_add_info("Nuit", led_rgb(0, 0, 24));
    led_db_add_info("Test", led_rgb(24, 24, 24));

    // Alarmes critiques
    led_db_add_alarm("Panne WIFI", 3, led_rgb(48, 0, 0));
    led_db_add_alarm("Panne NTP", 3, led_rgb(48, 0, 16));
    led_db_add_alarm("Absence METEO", 2, led_rgb(32, 0, 32));
    led_db_add_alarm("Erreur Ecriture SD", 4, led_rgb(48, 0, 0));

    // Alarmes de connectivite / configuration
    led_db_add_alarm("Erreur mot de passe WiFi", 2, led_rgb(48, 20, 0));
    led_db_add_alarm("Box WiFi introuvable", 2, led_rgb(40, 12, 0));

    // Alarmes d'attente / transitoires
    led_db_add_alarm("Attente NTP", 1, led_rgb(24, 24, 0));

    // Alarmes metier thermostat
    led_db_add_alarm("Erreur capteur SHT31", 3, led_rgb(48, 0, 24));
    led_db_add_alarm("Capteur SHT31 absent", 3, led_rgb(48, 0, 24));
    led_db_add_alarm("Relais BLOQUE", 4, led_rgb(48, 0, 8));
    led_db_add_alarm("Protection anti-cycle relais", 2, led_rgb(40, 20, 0));
    led_db_add_alarm("Thermostat DESACTIVE", 1, led_rgb(0, 24, 24));
    led_db_add_alarm("Mode hors-gel actif", 1, led_rgb(0, 16, 40));
}

void led_db_add_info(const char *name, led_color_t color)
{
    if (info_count >= LED_DB_MAX_ITEMS) {
        ESP_LOGW(TAG, "Limite de %d infos atteinte", LED_DB_MAX_ITEMS);
        return;
    }

    strncpy(info_db[info_count].name, name, sizeof(info_db[info_count].name) - 1);
    info_db[info_count].name[sizeof(info_db[info_count].name) - 1] = '\0';
    info_db[info_count].color = color;
    info_count++;
    ESP_LOGI(TAG, "Info ajoutee: %s", name);
}

void led_db_add_alarm(const char *name, int blinks, led_color_t color)
{
    if (alarm_count >= LED_DB_MAX_ITEMS) {
        ESP_LOGW(TAG, "Limite de %d alarmes atteinte", LED_DB_MAX_ITEMS);
        return;
    }

    strncpy(alarm_db[alarm_count].name, name, sizeof(alarm_db[alarm_count].name) - 1);
    alarm_db[alarm_count].name[sizeof(alarm_db[alarm_count].name) - 1] = '\0';
    alarm_db[alarm_count].blinks = blinks;
    alarm_db[alarm_count].color = color;
    alarm_count++;
    ESP_LOGI(TAG, "Alarme ajoutee: %s", name);
}

void led_db_delete_by_name(const char *name)
{
    if (!name) {
        return;
    }

    for (int i = 0; i < info_count; i++) {
        if (strcmp(info_db[i].name, name) == 0) {
            for (int j = i; j < info_count - 1; j++) {
                info_db[j] = info_db[j + 1];
            }
            info_count--;
            ESP_LOGI(TAG, "Info '%s' supprimee", name);
            return;
        }
    }

    for (int i = 0; i < alarm_count; i++) {
        if (strcmp(alarm_db[i].name, name) == 0) {
            for (int j = i; j < alarm_count - 1; j++) {
                alarm_db[j] = alarm_db[j + 1];
            }
            alarm_count--;
            ESP_LOGI(TAG, "Alarme '%s' supprimee", name);
            return;
        }
    }

    ESP_LOGW(TAG, "Aucun element trouve avec le nom '%s'", name);
}

int led_db_get_info_count(void)
{
    return info_count;
}

int led_db_get_alarm_count(void)
{
    return alarm_count;
}

stored_info_t *led_db_get_info_by_idx(int idx)
{
    if (idx >= 0 && idx < info_count) {
        return &info_db[idx];
    }
    return NULL;
}

stored_alarm_t *led_db_get_alarm_by_idx(int idx)
{
    if (idx >= 0 && idx < alarm_count) {
        return &alarm_db[idx];
    }
    return NULL;
}

void led_db_print_status(void)
{
    ESP_LOGI(TAG, "--- Etat de la base LED ---");
    ESP_LOGI(TAG, "[Infos: %d]", info_count);
    for (int i = 0; i < info_count; i++) {
        ESP_LOGI(
            TAG,
            "  %d: %s (RGB: %d,%d,%d)",
            i,
            info_db[i].name,
            info_db[i].color.r,
            info_db[i].color.g,
            info_db[i].color.b);
    }

    ESP_LOGI(TAG, "[Alarmes: %d]", alarm_count);
    for (int i = 0; i < alarm_count; i++) {
        ESP_LOGI(
            TAG,
            "  %d: %s (Blinks: %d, RGB: %d,%d,%d)",
            i,
            alarm_db[i].name,
            alarm_db[i].blinks,
            alarm_db[i].color.r,
            alarm_db[i].color.g,
            alarm_db[i].color.b);
    }
}

void led_db_simulate(int info_idx, int alarm_idx)
{
    if (info_idx >= 0 && info_idx < info_count) {
        led_set_background(LED_MODE_FIXED, info_db[info_idx].color, 1000);
        ESP_LOGI(TAG, "Simulation ambiance: %s", info_db[info_idx].name);
    }

    if (alarm_idx >= 0 && alarm_idx < alarm_count) {
        alert_add(alarm_db[alarm_idx].name);
        ESP_LOGI(TAG, "Simulation alarme: %s", alarm_db[alarm_idx].name);
    } else {
        alert_clear_all();
    }
}

void led_db_init(void)
{
    info_count = 0;
    alarm_count = 0;
    memset(info_db, 0, sizeof(info_db));
    memset(alarm_db, 0, sizeof(alarm_db));
    led_db_register_defaults();
    ESP_LOGI(
        TAG,
        "LED DB initialisee en memoire uniquement (%d infos, %d alarmes)",
        info_count,
        alarm_count);
}

void led_db_save(void)
{
    ESP_LOGD(TAG, "LED DB save ignore (config code-driven)");
}

void led_db_load(void)
{
    ESP_LOGD(TAG, "LED DB load ignore (config code-driven)");
}

bool led_db_exists(const char *name)
{
    if (name == NULL) {
        return false;
    }

    for (int i = 0; i < info_count; i++) {
        if (strcmp(info_db[i].name, name) == 0) {
            return true;
        }
    }

    for (int i = 0; i < alarm_count; i++) {
        if (strcmp(alarm_db[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

int led_db_get_info_idx_by_name(const char *name)
{
    if (name == NULL) {
        return -1;
    }

    for (int i = 0; i < info_count; i++) {
        if (strcmp(info_db[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int led_db_get_alarm_idx_by_name(const char *name)
{
    if (name == NULL) {
        return -1;
    }

    for (int i = 0; i < alarm_count; i++) {
        if (strcmp(alarm_db[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}
