#include "esp_http_server.h"
#include "esp_log.h"
// #include "fb_storage.h"
#include "cJSON.h"

static const char *TAG = "WS_API_FB";

// --- HANDLER : LISTER ---
esp_err_t get_fb_list_handler(httpd_req_t *req) {
    // 1. Récupération de la liste allouée
    char* list = list_freebox_files();

    if (list == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "FTP Error");
        return ESP_FAIL;
    }

    // 2. Envoi de la réponse
    httpd_resp_set_type(req, "text/plain");
    esp_err_t res = httpd_resp_sendstr(req, list);

    // 3. Libération UNIQUE et mise à NULL
    ESP_LOGI(TAG, "Libération de la liste FTP...");
    free(list);
    list = NULL; // Sécurité pour éviter toute réutilisation

    return res;
}

// --- HANDLER : LIRE ---
esp_err_t get_fb_read_handler(httpd_req_t *req) {
    char filename[64];
    char query[128];

    // 1. Extraire la chaîne de requête (tout ce qui est après le '?')
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        // 2. Chercher la clé "file" dans cette chaîne
        if (httpd_query_key_value(query, "file", filename, sizeof(filename)) == ESP_OK) {
            
            // Nettoyage de sécurité (supprime d'éventuels \r ou \n)
            strtok(filename, "\r\n");

            ESP_LOGI("API_FREEBOX", "Demande de lecture : '%s'", filename);
            
            char* data = read_from_freebox(filename);
            if (data) {
                httpd_resp_set_type(req, "text/plain");
                esp_err_t res = httpd_resp_sendstr(req, data);
                free(data); // Libération après envoi
                return res;
            } else {
                ESP_LOGE("API_FREEBOX", "Contenu vide ou erreur FTP pour %s", filename);
            }
        }
    }

    ESP_LOGW("API_FREEBOX", "Fichier non trouvé ou URL malformée");
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Fichier non trouvé");
    return ESP_FAIL;
}

// --- HANDLER : SAUVEGARDER ---
esp_err_t post_fb_save_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    // 1. Vérification de sécurité sur la taille
    if (total_len >= 4096) { // Limite arbitraire pour protéger la RAM
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Fichier trop volumineux");
        return ESP_FAIL;
    }

    // 2. Allocation dynamique sécurisée
    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur RAM");
        return ESP_FAIL;
    }

    // 3. Réception robuste (boucle pour s'assurer de tout lire)
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            free(buf);
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    // 4. Parsing JSON
    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *file_item = cJSON_GetObjectItem(root, "file");
        cJSON *content_item = cJSON_GetObjectItem(root, "content");

        if (cJSON_IsString(file_item) && cJSON_IsString(content_item)) {
            ESP_LOGI(TAG, "Sauvegarde de : %s", file_item->valuestring);
            write_to_freebox(file_item->valuestring, content_item->valuestring);
            httpd_resp_sendstr(req, "Sauvegarde terminée");
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalide");
        }
        cJSON_Delete(root);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Erreur parsing JSON");
    }

    free(buf);
    return ESP_OK;
}

// --- HANDLER : EFFACER ---
esp_err_t delete_fb_handler(httpd_req_t *req) {
    char filename[64];
    char query[128];

    // 1. On récupère d'abord la query string (ce qui est après le ?)
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        // 2. On extrait le nom du fichier
        if (httpd_query_key_value(query, "file", filename, sizeof(filename)) == ESP_OK) {
            
            // Nettoyage radical des caractères invisibles (\r, \n ou espaces)
            for(int i = 0; filename[i]; i++) {
                if(filename[i] == '\r' || filename[i] == '\n' || filename[i] == ' ') {
                    filename[i] = '\0';
                    break;
                }
            }

            ESP_LOGI("API_DEL", "Fichier à supprimer : [%s]", filename);

            if (delete_from_freebox(filename) == ESP_OK) {
                return httpd_resp_sendstr(req, "Fichier effacé avec succès");
            } else {
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erreur FTP DELE");
            }
        }
    }

    ESP_LOGW("API_DEL", "Requête malformée");
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nom de fichier manquant");
}

void ws_register_freebox_api(httpd_handle_t server) {
    httpd_uri_t uri_fb_list = { .uri = "/api/freebox/list", .method = HTTP_GET, .handler = get_fb_list_handler };
    httpd_uri_t uri_fb_read = { .uri = "/api/freebox/read", .method = HTTP_GET, .handler = get_fb_read_handler };
    httpd_uri_t uri_fb_save = { .uri = "/api/freebox/save", .method = HTTP_POST, .handler = post_fb_save_handler };
    httpd_uri_t uri_fb_del  = { .uri = "/api/freebox/delete", .method = HTTP_DELETE, .handler = delete_fb_handler };

    httpd_register_uri_handler(server, &uri_fb_list);
    httpd_register_uri_handler(server, &uri_fb_read);
    httpd_register_uri_handler(server, &uri_fb_save);
    httpd_register_uri_handler(server, &uri_fb_del);
}
