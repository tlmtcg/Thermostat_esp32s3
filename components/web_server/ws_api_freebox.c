#include "esp_http_server.h"
#include "esp_log.h"
#include "freebox_ftp.h" // Nouvelle inclusion
#include "cJSON.h"
#include <stdlib.h>
#include "web_server_metrics.h"

static const char *TAG = "WS_API_FB";

// --- HANDLER : LISTER ---
esp_err_t get_fb_list_handler(httpd_req_t *req)
{
    // Allocation d'un buffer pour recevoir la liste brute (ls -l)
    char *list_buf = malloc(4096);
    if (list_buf == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "RAM Error");
        return ESP_FAIL;
    }

    if (freebox_ftp_list(list_buf, 4096) != ESP_OK)
    {
        free(list_buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "FTP List Error");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    esp_err_t res = httpd_resp_sendstr(req, list_buf);

    free(list_buf);
    return res;
}

// --- HANDLER : LIRE ---
esp_err_t get_fb_read_handler(httpd_req_t *req)
{
    char filename[64];
    char query[128];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        if (httpd_query_key_value(query, "file", filename, sizeof(filename)) == ESP_OK)
        {

            strtok(filename, "\r\n "); // Nettoyage simple

            // Allocation d'un buffer pour le contenu du fichier
            size_t buf_size = 4096;
            char *data = malloc(buf_size);
            size_t bytes_read = 0;

            if (!data)
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "RAM Error");

            ESP_LOGI(TAG, "Lecture FTP : '%s'", filename);

            if (freebox_ftp_download(filename, data, buf_size, &bytes_read) == ESP_OK)
            {
                httpd_resp_set_type(req, "text/plain");
                esp_err_t res = httpd_resp_send(req, data, bytes_read);
                free(data);
                return res;
            }
            free(data);
        }
    }

    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Fichier non trouvé");
}

// --- HANDLER : SAUVEGARDER ---
esp_err_t post_fb_save_handler(httpd_req_t *req)
{
    int total_len = req->content_len;

    if (total_len <= 0 || total_len >= 8192)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Taille invalide");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "RAM Error");

    int received = httpd_req_recv(req, buf, total_len);
    if (received <= 0)
    {
        free(buf);
        return ESP_FAIL;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    esp_err_t ret = ESP_FAIL;

    if (root)
    {
        cJSON *file_item = cJSON_GetObjectItem(root, "file");
        cJSON *content_item = cJSON_GetObjectItem(root, "content");

        if (cJSON_IsString(file_item) && cJSON_IsString(content_item))
        {
            ESP_LOGI(TAG, "Upload FTP : %s", file_item->valuestring);

            if (freebox_ftp_upload(file_item->valuestring,
                                   content_item->valuestring,
                                   strlen(content_item->valuestring)) == ESP_OK)
            {
                httpd_resp_sendstr(req, "Upload OK");
                ret = ESP_OK;
            }
            else
            {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "FTP Upload Failed");
            }
        }
        cJSON_Delete(root);
    }

    free(buf);
    return ret;
}

// --- HANDLER : EFFACER ---
esp_err_t delete_fb_handler(httpd_req_t *req)
{
    char filename[64];
    char query[128];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        if (httpd_query_key_value(query, "file", filename, sizeof(filename)) == ESP_OK)
        {

            strtok(filename, "\r\n "); // Nettoyage

            ESP_LOGI(TAG, "Suppression FTP : %s", filename);

            if (freebox_ftp_delete(filename) == ESP_OK)
            {
                return httpd_resp_sendstr(req, "Fichier effacé");
            }
            else
            {
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur FTP DELE");
            }
        }
    }
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nom manquant");
}

// --- HANDLER : RENOMMER ---
esp_err_t post_fb_rename_handler(httpd_req_t *req)
{
    char buf[256];
    httpd_req_recv(req, buf, req->content_len);
    buf[req->content_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root)
    {
        cJSON *old_n = cJSON_GetObjectItem(root, "old_name");
        cJSON *new_n = cJSON_GetObjectItem(root, "new_name");
        if (cJSON_IsString(old_n) && cJSON_IsString(new_n))
        {
            freebox_ftp_rename(old_n->valuestring, new_n->valuestring);
            httpd_resp_sendstr(req, "OK");
        }
        cJSON_Delete(root);
    }
    return ESP_OK;
}

esp_err_t ws_register_freebox_api(httpd_handle_t server)
{
    // ESP_LOGI(TAG, "=== WS_API_FREEBOX: START REGISTER ===");

    g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    esp_err_t err;

    // ---------------- LIST ----------------
    httpd_uri_t uri_fb_list = {
        .uri = "/api/freebox/list",
        .method = HTTP_GET,
        .handler = get_fb_list_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET)", uri_fb_list.uri);

    err = httpd_register_uri_handler(server, &uri_fb_list);
    ESP_LOGI(TAG, "Result /list -> %s", esp_err_to_name(err));

    // ---------------- READ ----------------
    httpd_uri_t uri_fb_read = {
        .uri = "/api/freebox/read",
        .method = HTTP_GET,
        .handler = get_fb_read_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (GET)", uri_fb_read.uri);

    err = httpd_register_uri_handler(server, &uri_fb_read);
    ESP_LOGI(TAG, "Result /read -> %s", esp_err_to_name(err));

    // ---------------- SAVE ----------------
    httpd_uri_t uri_fb_save = {
        .uri = "/api/freebox/save",
        .method = HTTP_POST,
        .handler = post_fb_save_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST)", uri_fb_save.uri);

    err = httpd_register_uri_handler(server, &uri_fb_save);
    ESP_LOGI(TAG, "Result /save -> %s", esp_err_to_name(err));

    // ---------------- RENAME ----------------
    httpd_uri_t uri_fb_rename = {
        .uri = "/api/freebox/rename",
        .method = HTTP_POST,
        .handler = post_fb_rename_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (POST)", uri_fb_rename.uri);

    err = httpd_register_uri_handler(server, &uri_fb_rename);
    ESP_LOGI(TAG, "Result /rename -> %s", esp_err_to_name(err));

    // ---------------- DELETE ----------------
    httpd_uri_t uri_fb_del = {
        .uri = "/api/freebox/delete",
        .method = HTTP_DELETE,
        .handler = delete_fb_handler,
        .user_ctx = NULL};

    ESP_LOGI(TAG, "Register: %s (DELETE)", uri_fb_del.uri);

    err = httpd_register_uri_handler(server, &uri_fb_del);
    ESP_LOGI(TAG, "Result /delete -> %s", esp_err_to_name(err));

    // ---------------- FINAL ----------------

    g_http_handlers_used += 1;
    // ESP_LOGI(TAG, "HTTP usage: %d/%d", g_http_handlers_used, g_http_handlers_max);

    // ESP_LOGI(TAG, "=== WS_API_FREEBOX: END REGISTER ===");

    return ESP_OK;
}
