#include "ws_api_sd.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include <sys/stat.h>
#include "ff.h"
#include "web_server_metrics.h"

static const char *TAG = "WS_API_SD";
#define MOUNT_POINT "/sdcard"

// --- Handler : Lister les fichiers ---
static esp_err_t sd_list_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"files\":[");

    DIR *dir = opendir(MOUNT_POINT);
    if (!dir)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "SD Card not mounted");
    }

    struct dirent *entry;
    bool first = true;
    char json_entry[512];

    while ((entry = readdir(dir)) != NULL)
    {
        // Ignorer . et ..
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        // Ignorer les noms dangereux
        if (strchr(entry->d_name, '/'))
            continue;

        if (!first)
            httpd_resp_sendstr_chunk(req, ",");

        struct stat st;
        char full_path[320];
        snprintf(full_path, sizeof(full_path), "%s/%s",
                 MOUNT_POINT, entry->d_name);

        long f_size = 0;
        const char *f_type = "file";

        if (stat(full_path, &st) == 0)
        {
            f_size = (long)st.st_size;
            if (S_ISDIR(st.st_mode))
                f_type = "dir";
        }

        snprintf(json_entry, sizeof(json_entry),
                 "{\"name\":\"%s\",\"type\":\"%s\",\"size\":%ld}",
                 entry->d_name, f_type, f_size);

        httpd_resp_sendstr_chunk(req, json_entry);
        first = false;
    }

    closedir(dir);

    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// --- Handler : Lire un fichier (ex: /api/sd/read?file=alerts.log) ---
static esp_err_t sd_read_handler(httpd_req_t *req)
{
    char filename[64];
    char query[128];

    // 1. Récupérer la chaîne de requête
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Aucun parametre fourni");

    // 2. Extraire "file"
    if (httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cle 'file' non trouvee dans l'URL");

    // 3. Construire le chemin complet
    char full_path[256];
    snprintf(full_path, sizeof(full_path), MOUNT_POINT "/%s", filename);

    FILE *f = fopen(full_path, "r");
    if (!f)
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Fichier introuvable");

    httpd_resp_set_type(req, "text/plain");

    // 4. Lecture ligne par ligne (correct pour JSON Lines)
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        httpd_resp_sendstr_chunk(req, line);
    }

    fclose(f);

    // 5. Terminer la réponse chunkée
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// --- Handler : Créer ou Écraser un fichier (POST) ---
// Utilisation : /api/sd/write?file=test.txt avec le contenu dans le corps (body)
static esp_err_t sd_write_handler(httpd_req_t *req)
{
    char filename[64];
    char query[128];
    char buf[256];
    int ret, remaining = req->content_len;

    // 1. Lire la query string
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parametre 'file' manquant");
    }

    // 2. Construire le chemin complet
    char full_path[256];
    snprintf(full_path, sizeof(full_path), MOUNT_POINT "/%s", filename);

    // 3. Ouvrir le fichier
    FILE *f = fopen(full_path, "w");
    if (!f)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur ouverture fichier");

    // 4. Lire le corps HTTP et écrire dans le fichier
    while (remaining > 0)
    {
        ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                continue;

            fclose(f);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur reception donnees");
        }

        fwrite(buf, 1, ret, f);
        remaining -= ret;
    }

    fclose(f);

    // 5. Réponse OK
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Fichier enregistre avec succes");

    return ESP_OK;
}

// --- Handler : Renommer un fichier (PUT) ---
// Utilisation : /api/sd/rename?old=ancien.txt&new=nouveau.txt
static esp_err_t sd_rename_handler(httpd_req_t *req)
{
    char query[256], old_n[64], new_n[64];
    char path_old[128], path_new[128];

    // 1. Lire les paramètres
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "old", old_n, sizeof(old_n)) != ESP_OK ||
        httpd_query_key_value(query, "new", new_n, sizeof(new_n)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Parametres 'old' ou 'new' manquants");
    }

    // 2. Sécurité : empêcher les chemins dangereux
    if (strchr(old_n, '/') || strchr(new_n, '/'))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Nom de fichier invalide");
    }

    // 3. Construire les chemins complets
    snprintf(path_old, sizeof(path_old), MOUNT_POINT "/%s", old_n);
    snprintf(path_new, sizeof(path_new), MOUNT_POINT "/%s", new_n);

    // 4. Renommage
    if (rename(path_old, path_new) != 0)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Echec du renommage");
    }

    // 5. Réponse OK
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Fichier renomme avec succes");

    return ESP_OK;
}

// --- Handler : Supprimer un fichier (DELETE) ---
// Utilisation : /api/sd/delete?file=test.txt
static esp_err_t sd_delete_handler(httpd_req_t *req)
{
    char query[128], filename[64], full_path[128];

    // 1. Lire la query string
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Parametre 'file' manquant");
    }

    // 2. Sécurité : empêcher les chemins dangereux
    if (strchr(filename, '/'))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Nom de fichier invalide");
    }

    // 3. Construire le chemin complet
    snprintf(full_path, sizeof(full_path), MOUNT_POINT "/%s", filename);

    // 4. Supprimer via FatFS
    if (f_unlink(full_path) != FR_OK)
    {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND,
                                   "Fichier non trouve ou erreur");
    }

    // 5. Réponse OK
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Fichier supprime");

    return ESP_OK;
}

// --- Handler pour créer un dossier (POST) ---
// URL attendue : /api/sd/mkdir?path=nom_du_dossier
static esp_err_t sd_mkdir_handler(httpd_req_t *req)
{
    char query[128], dirname[64];

    // 1. Lire la query string
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "path", dirname, sizeof(dirname)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Parametre 'path' manquant");
    }

    // 2. Sécurité : empêcher les chemins dangereux
    if (strchr(dirname, '/'))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Nom de dossier invalide");
    }

    // 3. Appel logique dans sd_card.c
    esp_err_t res = sd_create_dir(dirname);

    if (res != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Echec de creation du dossier");
    }

    // 4. Réponse OK
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Dossier cree avec succes");

    return ESP_OK;
}

// --- Handler pour supprimer un dossier (DELETE) ---
// URL attendue : /api/sd/rmdir?path=nom_du_dossier
static esp_err_t sd_rmdir_handler(httpd_req_t *req)
{
    char query[128], dirname[64];

    // 1. Lire la query string
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "path", dirname, sizeof(dirname)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Parametre 'path' manquant");
    }

    // 2. Sécurité : empêcher les chemins dangereux
    if (strchr(dirname, '/'))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "Nom de dossier invalide");
    }

    // 3. Appel logique dans sd_card.c
    esp_err_t res = sd_remove_dir(dirname);

    if (res != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Echec suppression (dossier non vide ?)");
    }

    // 4. Réponse OK
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Dossier supprime avec succes");

    return ESP_OK;
}

// --- Enregistrement des URIs ---
esp_err_t ws_register_sd_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_SD: START REGISTER ===");

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    esp_err_t err;

    // ---------------- LIST ----------------
    httpd_uri_t list_uri = {
        .uri = "/api/sd/list",
        .method = HTTP_GET,
        .handler = sd_list_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (LIST GET)", list_uri.uri);

    err = httpd_register_uri_handler(server, &list_uri);
    ESP_LOGI(TAG, "Result /list -> %s", esp_err_to_name(err));

    // ---------------- READ ----------------
    httpd_uri_t read_uri = {
        .uri = "/api/sd/read",
        .method = HTTP_GET,
        .handler = sd_read_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (READ GET)", read_uri.uri);

    err = httpd_register_uri_handler(server, &read_uri);
    ESP_LOGI(TAG, "Result /read -> %s", esp_err_to_name(err));

    // ---------------- WRITE ----------------
    httpd_uri_t write_uri = {
        .uri = "/api/sd/write",
        .method = HTTP_POST,
        .handler = sd_write_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (WRITE POST)", write_uri.uri);

    err = httpd_register_uri_handler(server, &write_uri);
    ESP_LOGI(TAG, "Result /write -> %s", esp_err_to_name(err));

    // ---------------- RENAME ----------------
    httpd_uri_t rename_uri = {
        .uri = "/api/sd/rename",
        .method = HTTP_PUT,
        .handler = sd_rename_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (RENAME PUT)", rename_uri.uri);

    err = httpd_register_uri_handler(server, &rename_uri);
    ESP_LOGI(TAG, "Result /rename -> %s", esp_err_to_name(err));

    // ---------------- DELETE ----------------
    httpd_uri_t delete_uri = {
        .uri = "/api/sd/delete",
        .method = HTTP_DELETE,
        .handler = sd_delete_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (DELETE)", delete_uri.uri);

    err = httpd_register_uri_handler(server, &delete_uri);
    ESP_LOGI(TAG, "Result /delete -> %s", esp_err_to_name(err));

    // ---------------- MKDIR ----------------
    httpd_uri_t mkdir_uri = {
        .uri = "/api/sd/mkdir",
        .method = HTTP_POST,
        .handler = sd_mkdir_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (MKDIR POST)", mkdir_uri.uri);

    err = httpd_register_uri_handler(server, &mkdir_uri);
    ESP_LOGI(TAG, "Result /mkdir -> %s", esp_err_to_name(err));

    // ---------------- RMDIR ----------------
    httpd_uri_t rmdir_uri = {
        .uri = "/api/sd/rmdir",
        .method = HTTP_DELETE,
        .handler = sd_rmdir_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (RMDIR DELETE)", rmdir_uri.uri);

    err = httpd_register_uri_handler(server, &rmdir_uri);
    ESP_LOGI(TAG, "Result /rmdir -> %s", esp_err_to_name(err));

    // ---------------- FINAL ----------------

    // g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_SD: END REGISTER ===");

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "SD API registration FAILED");
        return err;
    }

    ESP_LOGI(TAG, "API SD enregistrée avec succès");
    return ESP_OK;
}
