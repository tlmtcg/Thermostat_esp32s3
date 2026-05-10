#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "weather.h"
#include "esp_crt_bundle.h"
#include "alert_manager.h"

static const char *TAG = "WEATHER_SERVICE";
static char *response_data = NULL;
static int response_len = 0;
#define MAX_HTTP_RECV_BUFFER 25600

weather_data_t latest_weather;

// Ce code accumule les données reçues par fragments lors d’une requête HTTP
// et les stocke dans un buffer dynamique (response_data)
// jusqu’à atteindre une taille maximale.
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA)
    // Un fragment de la réponse HTTP vient d’arriver.
    {
        if (response_len + evt->data_len < MAX_HTTP_RECV_BUFFER)
        // Sécurité : évite un dépassement de buffer
        {
            // On agrandit response_data pour y ajouter le nouveau fragment
            char *new_ptr = realloc(response_data, response_len + evt->data_len + 1);
            if (new_ptr)
            {
                // copie le fragment reçu à la suite du buffer existant
                response_data = new_ptr;
                memcpy(response_data + response_len, evt->data, evt->data_len);
                // met à jour la longueur totale
                response_len += evt->data_len;
                // le buffer devient une chaîne C valide
                response_data[response_len] = '\0';
            }
            else
            {
                ESP_LOGE(TAG, "Échec realloc buffer HTTP");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Buffer HTTP plein, données ignorées");
        }
    }
    return ESP_OK;
}

esp_err_t http_get_to_buffer(const char *url, int timeout_ms)
{
    // Reset buffer global
    // Avant chaque requête, on remet à zéro :
    // response_data (buffer dynamique)
    // response_len (taille accumulée)
    // Evite les fuites mémoire entre deux appels.

    if (response_data)
        free(response_data);
    response_data = NULL;
    response_len = 0;

    // Configuration du client http
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler, // accumule les fragments de réponse
        .timeout_ms = timeout_ms,
        .skip_cert_common_name_check = true, // ignore CN du certificat
    };

    ESP_LOGI(TAG, "Lancement de la requête url %s", url);
    // Lancement de la requête
    // Ouvre la connexion, envoie la requête, reçoit la réponse par fragments
    // Appelle _http_event_handler à chaque fragment
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    esp_http_client_cleanup(client);

    if (err != ESP_OK)
    {
        alert_add("Absence météo"); // La LED clignote en erreur
        return err;
    }

    // Vérification que la réponse existe
    if (response_data == NULL)
        return ESP_FAIL;

    alert_remove("Absence météo"); // La LED clignote en erreur
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
    const char *url =
        "https://api.open-meteo.com/v1/forecast?"
        "latitude=50.75&longitude=3.12"
        "&current=temperature_2m,relative_humidity_2m,weather_code"
        "&hourly=temperature_2m,relative_humidity_2m,weather_code&forecast_hours=48"
        "&daily=weather_code,temperature_2m_max,relative_humidity_2m_max"
        "&timezone=auto&timeformat=unixtime";

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
            }

            ESP_LOGI(TAG, "Parsing complet des 48 points réussi");
        }
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
    response_len = 0;

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
        return err;

    // Conversion de la réponse texte ("8.4") en float
    ESP_LOGI(TAG, "Réponse brute Jeedom : '%s'", response_data);
    data->current.jee_temp = atof(response_data);
    ESP_LOGI(TAG, "Valeur Jeedom reçue : %.2f", data->current.jee_temp);

    free(response_data);
    response_data = NULL;
    response_len = 0;

    return ESP_OK;
}
