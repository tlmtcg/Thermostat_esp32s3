#include "ws_api_sd.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include <sys/stat.h>
#include "ff.h"

static const char *TAG = "WS_API_SD";
#define MOUNT_POINT "/sdcard"

// --- Handler : Lister les fichiers ---
static esp_err_t sd_list_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"files\": [");

    DIR *dir = opendir(MOUNT_POINT);
    if (!dir)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD Card not mounted");
    }

    struct dirent *entry;
    bool first = true;

    // On augmente la taille du buffer pour éviter l'erreur de troncature
    // 256 (nom) + 10 (type) + 20 (taille) + structure JSON = ~350 octets minimum
    char json_entry[512];

    while ((entry = readdir(dir)) != NULL)
    {
        if (!first)
            httpd_resp_sendstr_chunk(req, ",");

        struct stat st;
        char full_path[320];
        snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, entry->d_name);

        // On initialise st_size à 0 au cas où stat échoue
        long f_size = 0;
        const char *f_type = "file";

        if (stat(full_path, &st) == 0)
        {
            f_size = (long)st.st_size;
            if (S_ISDIR(st.st_mode))
                f_type = "dir";
        }

        // snprintf sécurisé avec un buffer assez large
        int len = snprintf(json_entry, sizeof(json_entry),
                           "{\"name\":\"%s\", \"type\":\"%s\", \"size\":%ld}",
                           entry->d_name, f_type, f_size);

        // Si jamais c'est trop long (ne devrait plus arriver avec 512), on log l'erreur
        if (len >= sizeof(json_entry))
        {
            ESP_LOGW(TAG, "Nom de fichier trop long, troncature JSON");
        }

        httpd_resp_sendstr_chunk(req, json_entry);
        first = false;
    }
    closedir(dir);

    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// --- Handler : Lire un fichier (ex: /api/sd/read?file=config.txt) ---
static esp_err_t sd_read_handler(httpd_req_t *req)
{
    char filename[64];
    char query[128];

    // 1. Récupérer la chaîne de requête (ce qui est après le ?)
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        // 2. Extraire la valeur de "file" depuis la chaîne extraite
        if (httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK)
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cle 'file' non trouvee dans l'URL");
        }
    }
    else
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Aucun parametre fourni");
    }

    // 3. Construction du chemin et lecture (le reste ne change pas)
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", filename);

    FILE *f = fopen(full_path, "r");
    if (!f)
    {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Fichier introuvable");
    }

    httpd_resp_set_type(req, "text/plain");
    char chunk[256];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), f)) > 0)
    {
        httpd_resp_send_chunk(req, chunk, read_bytes);
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
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

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parametre 'file' manquant");
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", filename);

    FILE *f = fopen(full_path, "w");
    if (!f)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur ouverture fichier");

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            fclose(f);
            return ESP_FAIL;
        }
        fwrite(buf, 1, ret, f);
        remaining -= ret;
    }

    fclose(f);
    httpd_resp_sendstr(req, "Fichier enregistre avec succes");
    return ESP_OK;
}

// --- Handler : Renommer un fichier (PUT) ---
// Utilisation : /api/sd/rename?old=ancien.txt&new=nouveau.txt
static esp_err_t sd_rename_handler(httpd_req_t *req)
{
    char query[256], old_n[64], new_n[64];
    char path_old[128], path_new[128];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "old", old_n, sizeof(old_n)) != ESP_OK ||
        httpd_query_key_value(query, "new", new_n, sizeof(new_n)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parametres 'old' ou 'new' manquants");
    }

    snprintf(path_old, sizeof(path_old), "/sdcard/%s", old_n);
    snprintf(path_new, sizeof(path_new), "/sdcard/%s", new_n);

    if (rename(path_old, path_new) != 0)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Echec du renommage");
    }

    httpd_resp_sendstr(req, "Fichier renomme");
    return ESP_OK;
}

// --- Handler : Supprimer un fichier (DELETE) ---
// Utilisation : /api/sd/delete?file=test.txt
static esp_err_t sd_delete_handler(httpd_req_t *req)
{
    char query[128], filename[64], full_path[128];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parametre 'file' manquant");
    }

    snprintf(full_path, sizeof(full_path), "/sdcard/%s", filename);

    if (f_unlink(full_path) != FR_OK)

    {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Fichier non trouve ou erreur");
    }

    httpd_resp_sendstr(req, "Fichier supprime");
    return ESP_OK;
}

// --- Handler pour créer un dossier (POST) ---
// URL attendue : /api/sd/mkdir?path=nom_du_dossier
static esp_err_t sd_mkdir_handler(httpd_req_t *req) {
    char query[128], path[64];

    // Extraction des paramètres de l'URL
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "path", path, sizeof(path)) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parametre 'path' manquant");
    }

    // Appel de la fonction logique dans sd_card.c
    if (sd_create_dir(path) == ESP_OK) {
        httpd_resp_sendstr(req, "Dossier cree avec succes");
        return ESP_OK;
    } else {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Echec de creation du dossier");
    }
}

// --- Handler pour supprimer un dossier (DELETE) ---
// URL attendue : /api/sd/rmdir?path=nom_du_dossier
static esp_err_t sd_rmdir_handler(httpd_req_t *req) {
    char query[128], path[64];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "path", path, sizeof(path)) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parametre 'path' manquant");
    }

    // Appel de la fonction logique dans sd_card.c
    if (sd_remove_dir(path) == ESP_OK) {
        httpd_resp_sendstr(req, "Dossier supprime avec succes");
        return ESP_OK;
    } else {
        // Souvent en échec si le dossier n'est pas vide
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Echec suppression (dossier non vide ?)");
    }
}

// --- Enregistrement des URIs ---
esp_err_t ws_register_sd_api(httpd_handle_t server)
{
    // --- Définition des URIs ---
    
    // Lecture et Listing
    httpd_uri_t list_uri = {
        .uri      = "/api/sd/list",
        .method   = HTTP_GET,
        .handler  = sd_list_handler
    };

    httpd_uri_t read_uri = {
        .uri      = "/api/sd/read",
        .method   = HTTP_GET,
        .handler  = sd_read_handler
    };

    // Manipulation de fichiers
    httpd_uri_t write_uri = {
        .uri      = "/api/sd/write",
        .method   = HTTP_POST,
        .handler  = sd_write_handler
    };

    httpd_uri_t rename_uri = {
        .uri      = "/api/sd/rename",
        .method   = HTTP_PUT,
        .handler  = sd_rename_handler
    };

    httpd_uri_t delete_uri = {
        .uri      = "/api/sd/delete",
        .method   = HTTP_DELETE,
        .handler  = sd_delete_handler
    };

    // Gestion des dossiers
    httpd_uri_t mkdir_uri = {
        .uri      = "/api/sd/mkdir",
        .method   = HTTP_POST,
        .handler  = sd_mkdir_handler
    };

    httpd_uri_t rmdir_uri = {
        .uri      = "/api/sd/rmdir",
        .method   = HTTP_DELETE,
        .handler  = sd_rmdir_handler
    };

    // --- Enregistrement des Handlers ---
    
    httpd_register_uri_handler(server, &list_uri);
    httpd_register_uri_handler(server, &read_uri);
    httpd_register_uri_handler(server, &write_uri);
    httpd_register_uri_handler(server, &rename_uri);
    httpd_register_uri_handler(server, &delete_uri);
    httpd_register_uri_handler(server, &mkdir_uri);
    httpd_register_uri_handler(server, &rmdir_uri);

    ESP_LOGI(TAG, "API SD enregistrée : Full File Manager Ready");
    return ESP_OK;
}
