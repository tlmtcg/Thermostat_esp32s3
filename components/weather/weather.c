#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "weather.h"
#include "esp_crt_bundle.h"
#include "alert_manager.h"
#include "thermostat.h"
#include "config_runtime.h"
#include "weather_store.h"

static const char *TAG = "WEATHER_SERVICE";
static char *response_data = NULL;
static int weather_response_len = 0;
#define MAX_HTTP_RECV_BUFFER 25600

weather_data_t latest_weather;

// Ce code accumule les données reçues par fragments lors d’une requête HTTP
// et les stocke dans un buffer dynamique (response_data)
// jusqu’à atteindre une taille maximale.
// Handler d'événements HTTP (doit être défini quelque part)
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        // Allouer ou réallouer le buffer pour les nouvelles données
        char *new_data = realloc(response_data, weather_response_len + evt->data_len + 1);
        if (new_data == NULL)
        {
            ESP_LOGE(TAG, "Échec de l'allocation mémoire pour la réponse");
            free(response_data);
            response_data = NULL;
            weather_response_len = 0;
            return ESP_FAIL;
        }
        response_data = new_data;
        memcpy(response_data + weather_response_len, evt->data, evt->data_len);
        weather_response_len += evt->data_len;
        response_data[weather_response_len] = '\0'; // Terminaison nulle
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t http_get_to_buffer(const char *url, int timeout_ms)
{
    // Réinitialiser le buffer avant chaque requête
    if (response_data != NULL)
    {
        free(response_data);
        response_data = NULL;
        weather_response_len = 0;
    }

    // Configuration du client HTTP
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = timeout_ms,
    };

    ESP_LOGI(TAG, "Lancement de la requête URL: %s", url);

    // Initialiser le client HTTP
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Échec de l'initialisation du client HTTP");
        alert_add("Absence METEO");
        return ESP_FAIL;
    }

    // Exécuter la requête
    esp_err_t err = esp_http_client_perform(client);

    // Nettoyer le client HTTP
    esp_http_client_cleanup(client);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erreur HTTP: %s", esp_err_to_name(err));
        alert_add("Absence METEO");
        return err;
    }

    // Vérifier que la réponse existe et n'est pas vide
    if (response_data == NULL || weather_response_len == 0)
    {
        ESP_LOGE(TAG, "Aucune donnée reçue");
        alert_add("Absence METEO");
        return ESP_FAIL;
    }

    // Succès : supprimer l'alerte si elle existait
    alert_remove("Absence METEO");
    return ESP_OK;
}

const char *get_weather_description(int code)
{
    switch (code)
    {
    case 0:
        return "Ciel clair ☀️";
    case 1:
        return "Principalement clair 🌤️";
    case 2:
        return "Partiellement nuageux ⛅";
    case 3:
        return "Couvert ☁️";
    case 45:
    case 48:
        return "Brouillard 🌫️";
    case 51:
    case 53:
    case 55:
        return "Bruine 🌧️";
    case 61:
    case 63:
    case 65:
        return "Pluie 🌧️";
    case 71:
    case 73:
    case 75:
        return "Neige ❄️";
    case 80:
    case 81:
    case 82:
        return "Averses 🌦️";
    case 95:
    case 96:
    case 99:
        return "Orage ⛈️";
    default:
        return "Inconnu 🤷";
    }
}

// Fonction complète de récupération météo, avec gestion du current, hourly et daily
esp_err_t weather_update(weather_data_t *data)
{
    // Sécurité : si le pointeur est NULL → erreur immédiate
    if (!data)
        return ESP_ERR_INVALID_ARG;

    // URL optimisée pour un seul appel.
    // current : température, humidité, code météo
    // hourly : mêmes données sur 48h
    // daily : météo sur 7 jours
    // URL optimisée pour un seul appel
    char url[512];

    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?"
             "latitude=%.5f&longitude=%.5f"
             "&current=temperature_2m,relative_humidity_2m,weather_code"
             "&hourly=temperature_2m,relative_humidity_2m,weather_code&forecast_hours=48"
             "&daily=weather_code,temperature_2m_max,relative_humidity_2m_max"
             "&timezone=auto&timeformat=unixtime",
             g_cfg.weather_lat,
             g_cfg.weather_lon);

    // Appel de la fonction commune
    esp_err_t err = http_get_to_buffer(url, 20000);
    if (err != ESP_OK)
        return err;

    // Parsing JSON
    cJSON *root = cJSON_Parse(response_data);
    if (!root)
    {
        ESP_LOGE(TAG, "Erreur de parsing JSON");
        err = ESP_FAIL;
    }

    // 1. Parsing du bloc CURRENT
    cJSON *cur = cJSON_GetObjectItem(root, "current");
    if (cur)
    {
        data->current.timestamp = (long)cJSON_GetObjectItem(cur, "time")->valuedouble;
        data->current.temperature = cJSON_GetObjectItem(cur, "temperature_2m")->valuedouble;
        data->current.humidity = cJSON_GetObjectItem(cur, "relative_humidity_2m")->valuedouble;
        data->current.weather_code = cJSON_GetObjectItem(cur, "weather_code")->valueint;
        ESP_LOGI(TAG, "Parsing 'current' réussi");
    }
    else
    {
        ESP_LOGW(TAG, "Bloc 'current' manquant");
    }

    // 2. Parsing du bloc HOURLY (48h)
    cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
    if (hourly)
    {
        cJSON *t_arr = cJSON_GetObjectItem(hourly, "time");
        cJSON *temp_arr = cJSON_GetObjectItem(hourly, "temperature_2m");
        cJSON *hum_arr = cJSON_GetObjectItem(hourly, "relative_humidity_2m");
        cJSON *code_arr = cJSON_GetObjectItem(hourly, "weather_code");

        if (cJSON_IsArray(temp_arr) && cJSON_GetArraySize(temp_arr) > 0 &&
            cJSON_IsArray(hum_arr) && cJSON_GetArraySize(hum_arr) > 0)
        {
            // Extraction du premier élément (index 0) de chaque tableau (Heure en cours)
            cJSON *current_temp_item = cJSON_GetArrayItem(temp_arr, 0);
            cJSON *current_hum_item = cJSON_GetArrayItem(hum_arr, 0);
            cJSON *temp_1h_item = cJSON_GetArrayItem(temp_arr, 1);

            if (cJSON_IsNumber(current_temp_item) && cJSON_IsNumber(current_hum_item))
            {
                float temp_ext = (float)current_temp_item->valuedouble;
                float hum_ext = (float)current_hum_item->valuedouble;
                float temp_1h = (float)temp_1h_item->valuedouble;
                // Mise à jour du thermostat température dans une heure
                thermostat_update_forecast_data(temp_1h);

                // Mise à jour du thermostat avec des variables numériques simples et propres
                thermostat_update_outdoor_data(temp_ext, hum_ext);
            }
            else
            {
                ESP_LOGW(TAG, "Les éléments du tableau météo ne sont pas des nombres valides");
            }
        }
        int size = cJSON_GetArraySize(t_arr);

        if (size >= 48)
        {
            // 1. On garde votre point unique à l'index 47 (pour les logs/infos simples)
            data->forecast_48h.timestamp = (long)cJSON_GetArrayItem(t_arr, 47)->valuedouble;
            data->forecast_48h.temperature = cJSON_GetArrayItem(temp_arr, 47)->valuedouble;
            data->forecast_48h.humidity = cJSON_GetArrayItem(hum_arr, 47)->valuedouble;
            data->forecast_48h.weather_code = cJSON_GetArrayItem(code_arr, 47)->valueint;

            // 2. NOUVEAU : On remplit les tableaux complets pour le graphique API
            for (int i = 0; i < 48; i++)
            {
                data->forecast_48h_temp[i] = cJSON_GetArrayItem(temp_arr, i)->valuedouble;
                data->forecast_48h_hum[i] = cJSON_GetArrayItem(hum_arr, i)->valuedouble;
                data->forecast_48h_code[i] = cJSON_GetArrayItem(code_arr, i)->valueint;
            }

            ESP_LOGI(TAG, "Parsing complet des 48 points réussi");
        }
    }

    // Copier les tableaux dans la structure globale utilisée par le moteur prédictif
    for (int i = 0; i < 48; i++)
    {
        g_weather_data.forecast_48h_temp[i] = data->forecast_48h_temp[i];
        g_weather_data.forecast_48h_hum[i] = data->forecast_48h_hum[i];
        g_weather_data.forecast_48h_code[i] = data->forecast_48h_code[i];
    }

    // 3. Parsing du bloc DAILY (7 jours)
    cJSON *daily = cJSON_GetObjectItem(root, "daily");
    if (daily)
    {
        cJSON *t_arr = cJSON_GetObjectItem(daily, "time");
        cJSON *temp_arr = cJSON_GetObjectItem(daily, "temperature_2m_max");
        cJSON *hum_arr = cJSON_GetObjectItem(daily, "relative_humidity_2m_max");
        cJSON *code_arr = cJSON_GetObjectItem(daily, "weather_code");

        for (int i = 0; i < 7; i++)
        {
            data->forecast_7j[i].timestamp = (long)cJSON_GetArrayItem(t_arr, i)->valuedouble;
            data->forecast_7j[i].temperature = cJSON_GetArrayItem(temp_arr, i)->valuedouble;
            data->forecast_7j[i].weather_code = cJSON_GetArrayItem(code_arr, i)->valueint;
        }
    }

    // Nettoyage JSON
    cJSON_Delete(root);
    free(response_data);
    response_data = NULL;
    weather_response_len = 0;

    weather_store_set_all(data);

    return ESP_OK;
}

esp_err_t jeedom_temp_update(weather_data_t *data)
{
    if (!data)
        return ESP_ERR_INVALID_ARG;

    const char *url =
        "http://trever.freeboxos.fr:14254/core/api/jeeApi.php?"
        "apikey=d8RaIZcJA0iAaUkQGMVyLhk0rAZq2nGl&type=cmd&id=16109";

    esp_err_t err = http_get_to_buffer(url, 10000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erreur Jeedom");
        return err;
    }

    // Conversion de la réponse texte ("8.4") en float
    ESP_LOGI(TAG, "Réponse brute Jeedom : '%s'", response_data);
    data->current.jee_temp = atof(response_data);
    ESP_LOGI(TAG, "Valeur Jeedom reçue : %.2f", data->current.jee_temp);

    free(response_data);
    response_data = NULL;
    weather_response_len = 0;

    return ESP_OK;
}

float temperature_get_outdoor()
{
    return latest_weather.current.jee_temp;
}

weather_data_t g_weather_data = {0};

float weather_get_forecast_temp(int hours)
{
    if (hours < 0 || hours >= 48)
    {
        ESP_LOGW(TAG, "Temp forecast: index %d hors limites, retour 0", hours);
        return 0.0f;
    }

    float v = g_weather_data.forecast_48h_temp[hours];
    ESP_LOGD(TAG, "Temp forecast +%dh = %.2f°C", hours, v);
    return v;
}

float weather_get_forecast_humidity(int hours)
{
    if (hours < 0 || hours >= 48)
    {
        ESP_LOGW(TAG, "Hum forecast: index %d hors limites, retour 0", hours);
        return 0.0f;
    }

    float v = g_weather_data.forecast_48h_hum[hours];
    ESP_LOGD(TAG, "Hum forecast +%dh = %.1f%%", hours, v);
    return v;
}

int weather_get_current_code(void)
{
    return g_weather_data.current.weather_code;
}

int weather_get_forecast_code(int hours)
{
    if (hours < 0 || hours >= 48)
    {
        ESP_LOGW("WEATHER", "forecast_code: index %d hors limites", hours);
        return g_weather_data.current.weather_code; // fallback
    }

    return g_weather_data.forecast_48h_code[hours];
}

esp_err_t weather_geocode_city(const char *city, double *lat, double *lon)
{
    if (!city || !lat || !lon)
        return ESP_ERR_INVALID_ARG;

    char url[256];
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=fr&format=json",
             city);

    esp_err_t err = http_get_to_buffer(url, 20000);
    if (err != ESP_OK || response_data == NULL)
    {
        ESP_LOGE("WEATHER", "Erreur requête geocoding");
        return ESP_FAIL;
    }

    // Parse JSON depuis response_data
    cJSON *root = cJSON_Parse(response_data);
    if (!root)
    {
        ESP_LOGE("WEATHER", "JSON geocoding invalide");
        free(response_data);
        response_data = NULL;
        weather_response_len = 0;
        return ESP_FAIL;
    }

    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0)
    {
        cJSON_Delete(root);
        free(response_data);
        response_data = NULL;
        weather_response_len = 0;
        return ESP_FAIL;
    }

    cJSON *item = cJSON_GetArrayItem(results, 0);

    *lat = cJSON_GetObjectItem(item, "latitude")->valuedouble;
    *lon = cJSON_GetObjectItem(item, "longitude")->valuedouble;

    cJSON_Delete(root);

    // Libération du buffer HTTP
    free(response_data);
    response_data = NULL;
    weather_response_len = 0;

    return ESP_OK;
}

extern void weather_store_set_all(const weather_data_t *src);

void weather_update_task(void *arg)
{
    weather_data_t tmp;
    esp_err_t ret = weather_update(&tmp);

    if (ret == ESP_OK)
    {
        weather_store_set_all(&tmp);
        ESP_LOGI("WEATHER", "Mise à jour météo manuelle OK");
    }
    else
    {
        ESP_LOGE("WEATHER", "Échec mise à jour météo manuelle");
    }

    vTaskDelete(NULL);
}
