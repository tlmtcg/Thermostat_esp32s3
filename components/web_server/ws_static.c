#include "ws_static.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "WS_STATIC";

/*
 * 1. DÉCLARATION DES SYMBOLES BINAIRES
 * Ces symboles sont générés automatiquement par CMake via la commande EMBED_FILES.
 * Le nom suit toujours le schéma : _binary_[nom_du_fichier]_[start/end]
 * Les points (.) dans les noms de fichiers deviennent des underscores (_).
 */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t wifi_html_start[] asm("_binary_wifi_html_start");
extern const uint8_t wifi_html_end[] asm("_binary_wifi_html_end");
extern const uint8_t common_css_start[] asm("_binary_common_css_start");
extern const uint8_t common_css_end[] asm("_binary_common_css_end");
extern const uint8_t index_js_start[] asm("_binary_index_js_start");
extern const uint8_t index_js_end[] asm("_binary_index_js_end");
extern const uint8_t nav_html_start[] asm("_binary_nav_html_start");
extern const uint8_t nav_html_end[] asm("_binary_nav_html_end");
extern const uint8_t time_html_start[] asm("_binary_time_html_start");
extern const uint8_t time_html_end[] asm("_binary_time_html_end");
extern const uint8_t led_html_start[] asm("_binary_led_html_start");
extern const uint8_t led_html_end[] asm("_binary_led_html_end");
extern const uint8_t alarms_html_start[] asm("_binary_alarms_html_start");
extern const uint8_t alarms_html_end[] asm("_binary_alarms_html_end");
extern const uint8_t weather_html_start[] asm("_binary_weather_html_start");
extern const uint8_t weather_html_end[] asm("_binary_weather_html_end");
extern const uint8_t freebox_html_start[] asm("_binary_freebox_html_start");
extern const uint8_t freebox_html_end[] asm("_binary_freebox_html_end");
extern const uint8_t logs_html_start[] asm("_binary_logs_html_start");
extern const uint8_t logs_html_end[] asm("_binary_logs_html_end");
extern const uint8_t sys_html_start[] asm("_binary_sys_html_start");
extern const uint8_t sys_html_end[] asm("_binary_sys_html_end");
extern const uint8_t program_html_start[] asm("_binary_program_html_start");
extern const uint8_t program_html_end[] asm("_binary_program_html_end");
extern const uint8_t jeedom_html_start[] asm("_binary_jeedom_html_start");
extern const uint8_t jeedom_html_end[] asm("_binary_jeedom_html_end");

/**
 * @brief Fonction générique pour envoyer un fichier stocké en Flash.
 * @param type Le type MIME (ex: text/html). Ajoute auto le charset UTF-8 pour le texte.
 */
static esp_err_t send_embedded_file(httpd_req_t *req,
                                    const uint8_t *start,
                                    const uint8_t *end,
                                    const char *type)
{
    // Ajout automatique de l'encodage UTF-8 pour éviter les problèmes d'accents
    if (strstr(type, "text") || strstr(type, "javascript"))
    {
        char type_with_charset[64];
        snprintf(type_with_charset, sizeof(type_with_charset), "%s; charset=utf-8", type);
        httpd_resp_set_type(req, type_with_charset);
    }
    else
    {
        httpd_resp_set_type(req, type);
    }

    // Calcul de la taille : adresse de fin - adresse de début
    return httpd_resp_send(req, (const char *)start, end - start);
}

/*
 * 2. LES HANDLERS (Fonctions de rappel)
 * Chaque fonction correspond à une page spécifique.
 */

static esp_err_t get_index(httpd_req_t *req)
{
    return send_embedded_file(req, index_html_start, index_html_end, "text/html");
}

static esp_err_t get_time(httpd_req_t *req)
{
    return send_embedded_file(req, time_html_start, time_html_end, "text/html");
}

static esp_err_t get_wifi(httpd_req_t *req)
{
    return send_embedded_file(req, wifi_html_start, wifi_html_end, "text/html");
}

static esp_err_t get_css(httpd_req_t *req)
{
    return send_embedded_file(req, common_css_start, common_css_end, "text/css");
}

static esp_err_t get_js(httpd_req_t *req)
{
    return send_embedded_file(req, index_js_start, index_js_end, "application/javascript");
}

static esp_err_t get_nav(httpd_req_t *req)
{
    return send_embedded_file(req, nav_html_start, nav_html_end, "text/html");
}

static esp_err_t led_page_handler(httpd_req_t *req)
{
    return send_embedded_file(req, led_html_start, led_html_end, "text/html");
}

static esp_err_t alarms_page_handler(httpd_req_t *req)
{
    return send_embedded_file(req, alarms_html_start, alarms_html_end, "text/html");
}

static esp_err_t weather_html_handler(httpd_req_t *req)
{
    // Version manuelle pour l'exemple (identique à send_embedded_file)
    // const size_t sz = (weather_html_end - weather_html_start);
    // httpd_resp_set_type(req, "text/html; charset=utf-8");
    // return httpd_resp_send(req, (const char *)weather_html_start, sz);
    return send_embedded_file(req, weather_html_start, weather_html_end, "text/html");
}

static esp_err_t get_freebox(httpd_req_t *req) { return send_embedded_file(req, freebox_html_start, freebox_html_end, "text/html"); }

static esp_err_t get_logs(httpd_req_t *req)
{
    return send_embedded_file(req, logs_html_start, logs_html_end, "text/html");
}

static esp_err_t get_sys(httpd_req_t *req)
{
    return send_embedded_file(req, sys_html_start, sys_html_end, "text/html");
}

static esp_err_t get_program(httpd_req_t *req)
{
    return send_embedded_file(req, program_html_start, program_html_end, "text/html");
}

static esp_err_t get_jeedom(httpd_req_t *req)
{
    return send_embedded_file(req, jeedom_html_start, jeedom_html_end, "text/html");
}   

/**
 * 3. ENREGISTREMENT DES ROUTES
 * C'est ici qu'on associe une URL (ex: /sys) à un Handler (ex: get_sys).
 */
esp_err_t ws_register_static(httpd_handle_t server)
{
    // Définition des structures de l'URI
    // format: { uri, method, handler, user_ctx }
    httpd_uri_t uri_index = {.uri = "/", .method = HTTP_GET, .handler = get_index};
    httpd_uri_t uri_wifi = {.uri = "/wifi", .method = HTTP_GET, .handler = get_wifi};
    httpd_uri_t uri_css = {.uri = "/common.css", .method = HTTP_GET, .handler = get_css};
    httpd_uri_t uri_js = {.uri = "/index.js", .method = HTTP_GET, .handler = get_js};
    httpd_uri_t uri_nav = {.uri = "/nav.html", .method = HTTP_GET, .handler = get_nav};
    httpd_uri_t uri_time = {.uri = "/time", .method = HTTP_GET, .handler = get_time};
    httpd_uri_t led_ui_uri = {.uri = "/led", .method = HTTP_GET, .handler = led_page_handler};
    httpd_uri_t uri_weather = {.uri = "/weather", .method = HTTP_GET, .handler = weather_html_handler};
    httpd_uri_t uri_alarms = {.uri = "/alarms", .method = HTTP_GET, .handler = alarms_page_handler};
    httpd_uri_t uri_freebox = {.uri = "/freebox", .method = HTTP_GET, .handler = get_freebox};
    httpd_uri_t uri_logs = {.uri = "/logs", .method = HTTP_GET, .handler = get_logs};
    httpd_uri_t uri_sys = {.uri = "/sys", .method = HTTP_GET, .handler = get_sys};
    httpd_uri_t uri_program = {.uri = "/program", .method = HTTP_GET, .handler = get_program};
    httpd_uri_t uri_jeedom = {.uri = "/jeedom", .method = HTTP_GET, .handler = get_jeedom};

    // Enregistrement effectif auprès du serveur HTTP
    httpd_register_uri_handler(server, &uri_index);
    httpd_register_uri_handler(server, &uri_wifi);
    httpd_register_uri_handler(server, &uri_nav);
    httpd_register_uri_handler(server, &uri_css);
    httpd_register_uri_handler(server, &uri_js);
    httpd_register_uri_handler(server, &uri_time);
    httpd_register_uri_handler(server, &led_ui_uri);
    httpd_register_uri_handler(server, &uri_weather);
    httpd_register_uri_handler(server, &uri_alarms);
    httpd_register_uri_handler(server, &uri_freebox);
    httpd_register_uri_handler(server, &uri_logs);
    httpd_register_uri_handler(server, &uri_sys);
    httpd_register_uri_handler(server, &uri_program);
    httpd_register_uri_handler(server, &uri_jeedom);

    ESP_LOGI(TAG, "Handlers statiques enregistrés avec succès");
    return ESP_OK;
}
