#include "freebox_ftp.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "FREEBOX_FTP";
#define BUF_SIZE 1024

static freebox_ftp_config_t g_config = {
    .host = FREEBOX_FTP_HOST,
    .port = FREEBOX_FTP_PORT,
    .user = FREEBOX_FTP_USER,
    .pass = FREEBOX_FTP_PASS,
    .path = FREEBOX_FTP_PATH,
    .timeout_ms = FREEBOX_FTP_TIMEOUT_MS,
};

// --- Fonctions Utilitaires Internes ---

static int receive_response(int sock, char *res, size_t max_size)
{
    memset(res, 0, max_size);
    int bytes = recv(sock, res, max_size - 1, 0);
    if (bytes > 0)
    {
        res[bytes] = '\0';
        ESP_LOGI(TAG, "< %s", res); // Log de la réponse brute
    }
    else
    {
        ESP_LOGE(TAG, "Erreur de réception ou timeout");
    }
    return bytes;
}

static int send_command(int sock, const char *cmd, const char *arg, char *res, size_t res_size)
{
    char *buf = malloc(BUF_SIZE);
    if (!buf)
        return -1;

    int len = arg ? snprintf(buf, BUF_SIZE, "%s %s\r\n", cmd, arg)
                  : snprintf(buf, BUF_SIZE, "%s\r\n", cmd);

    ESP_LOGI(TAG, "> %.*s", len - 2, buf); // Log de la commande sans CRLF
    int ret = send(sock, buf, len, 0);
    free(buf);

    if (ret < 0)
        return -1;
    return receive_response(sock, res, res_size);
}

static int connect_data_port(char *pasv_res, struct timeval *tv)
{
    int a, b, c, d, p1, p2;
    char *p = strchr(pasv_res, '(');
    if (!p || sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &p1, &p2) != 6)
    {
        ESP_LOGE(TAG, "Erreur de parsing de la réponse PASV");
        return -1;
    }

    int port = (p1 << 8) | p2;
    ESP_LOGI(TAG, "Ouverture socket DATA sur %d.%d.%d.%d:%d", a, b, c, d, port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(g_config.host)};

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, tv, sizeof(*tv));
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        ESP_LOGE(TAG, "Connexion DATA échouée");
        close(sock);
        return -1;
    }
    return sock;
}

static int ftp_connect_and_auth(char *res_buf, struct timeval *tv)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    ESP_LOGI(TAG, "Connexion au serveur %s:%d...", g_config.host, g_config.port);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, tv, sizeof(*tv));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(g_config.port),
        .sin_addr.s_addr = inet_addr(g_config.host)};

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        ESP_LOGE(TAG, "Connexion socket commande impossible");
        goto fail;
    }

    if (receive_response(sock, res_buf, BUF_SIZE) <= 0 || !strstr(res_buf, "220"))
        goto fail;

    if (send_command(sock, "USER", g_config.user, res_buf, BUF_SIZE) <= 0)
        goto fail;
    if (send_command(sock, "PASS", g_config.pass, res_buf, BUF_SIZE) <= 0 || !strstr(res_buf, "230"))
    {
        ESP_LOGE(TAG, "Authentification échouée");
        goto fail;
    }

    ESP_LOGI(TAG, "Connecté et authentifié");
    send_command(sock, "CWD", g_config.path, res_buf, BUF_SIZE);
    send_command(sock, "TYPE", "I", res_buf, BUF_SIZE);

    return sock;
fail:
    close(sock);
    return -1;
}

// --- API Publique ---

esp_err_t freebox_ftp_list(char *buffer, size_t buffer_size)
{
    char *res = malloc(BUF_SIZE);
    if (!res)
        return ESP_ERR_NO_MEM;

    int sock = -1, d_sock = -1;
    esp_err_t ret = ESP_FAIL;
    struct timeval tv = {.tv_sec = g_config.timeout_ms / 1000};

    if ((sock = ftp_connect_and_auth(res, &tv)) < 0)
        goto cleanup_res;

    if (send_command(sock, "PASV", NULL, res, BUF_SIZE) <= 0 || (d_sock = connect_data_port(res, &tv)) < 0)
        goto cleanup;

    if (send_command(sock, "LIST", NULL, res, BUF_SIZE) <= 0 || !strstr(res, "150"))
        goto cleanup;

    int received = 0, total = 0;
    while ((received = recv(d_sock, buffer + total, buffer_size - total - 1, 0)) > 0)
    {
        total += received;
    }
    buffer[total] = '\0';

    close(d_sock);
    d_sock = -1;
    if (receive_response(sock, res, BUF_SIZE) > 0 && strstr(res, "226"))
    {
        ESP_LOGI(TAG, "Liste récupérée avec succès");
        ret = ESP_OK;
    }

cleanup:
    if (d_sock >= 0)
        close(d_sock);
    if (sock >= 0)
        close(sock);
cleanup_res:
    free(res);
    return ret;
}

esp_err_t freebox_ftp_delete(const char *filename)
{
    char *res = malloc(BUF_SIZE);
    if (!res)
        return ESP_ERR_NO_MEM;

    int sock = -1;
    esp_err_t ret = ESP_FAIL;
    struct timeval tv = {.tv_sec = g_config.timeout_ms / 1000};

    if ((sock = ftp_connect_and_auth(res, &tv)) < 0)
        goto cleanup_res;

    if (send_command(sock, "DELE", filename, res, BUF_SIZE) > 0 && strstr(res, "250"))
    {
        ESP_LOGI(TAG, "Fichier '%s' supprimé", filename);
        ret = ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Échec de la suppression du fichier '%s'", filename);
    }
    if (sock >= 0)
        close(sock);
cleanup_res:
    free(res);
    return ret;
}

esp_err_t freebox_ftp_rename(const char *old_name, const char *new_name)
{
    char *res = malloc(BUF_SIZE);
    if (!res)
        return ESP_ERR_NO_MEM;

    int sock = -1;
    esp_err_t ret = ESP_FAIL;
    struct timeval tv = {.tv_sec = g_config.timeout_ms / 1000};

    if ((sock = ftp_connect_and_auth(res, &tv)) < 0)
        goto cleanup_res;

    if (send_command(sock, "RNFR", old_name, res, BUF_SIZE) > 0 && strstr(res, "350"))
    {
        if (send_command(sock, "RNTO", new_name, res, BUF_SIZE) > 0 && strstr(res, "250"))
        {
            ESP_LOGI(TAG, "Fichier renommé avec succès");
            ret = ESP_OK;
        }
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Échec du renommage de '%s' en '%s'", old_name, new_name);
    }
    if (sock >= 0)
        close(sock);
cleanup_res:
    free(res);
    return ret;
}

esp_err_t freebox_ftp_upload(const char *filename, const char *data, size_t len)
{
    char *res = malloc(BUF_SIZE);
    if (!res)
        return ESP_ERR_NO_MEM;

    int sock = -1, d_sock = -1;
    esp_err_t ret = ESP_FAIL;
    struct timeval tv = {.tv_sec = g_config.timeout_ms / 1000};

    if ((sock = ftp_connect_and_auth(res, &tv)) < 0)
        goto cleanup_res;

    if (send_command(sock, "PASV", NULL, res, BUF_SIZE) <= 0 || (d_sock = connect_data_port(res, &tv)) < 0)
        goto cleanup;

    if (send_command(sock, "STOR", filename, res, BUF_SIZE) <= 0 || !strstr(res, "150"))
        goto cleanup;

    if (send(d_sock, data, len, 0) == len)
    {
        close(d_sock);
        d_sock = -1;
        if (receive_response(sock, res, BUF_SIZE) > 0 && strstr(res, "226"))
            ret = ESP_OK;
    }
cleanup:
    if (d_sock >= 0)
        close(d_sock);
    if (sock >= 0)
        close(sock);
cleanup_res:
    free(res);
    return ret;
}

esp_err_t freebox_ftp_download(const char *filename, char *buffer, size_t buffer_size, size_t *bytes_read)
{
    char *res = malloc(BUF_SIZE);
    if (!res)
        return ESP_ERR_NO_MEM;

    int sock = -1, d_sock = -1;
    esp_err_t ret = ESP_FAIL;
    struct timeval tv = {.tv_sec = g_config.timeout_ms / 1000};

    if ((sock = ftp_connect_and_auth(res, &tv)) < 0)
        goto cleanup_res;

    if (send_command(sock, "PASV", NULL, res, BUF_SIZE) <= 0 || (d_sock = connect_data_port(res, &tv)) < 0)
        goto cleanup;

    if (send_command(sock, "RETR", filename, res, BUF_SIZE) <= 0 || !strstr(res, "150"))
        goto cleanup;

    int received = 0, total = 0;
    while ((received = recv(d_sock, buffer + total, buffer_size - total - 1, 0)) > 0)
    {
        total += received;
    }
    *bytes_read = total;
    buffer[total] = '\0';

    close(d_sock);
    d_sock = -1;
    if (receive_response(sock, res, BUF_SIZE) > 0 && strstr(res, "226"))
        ret = ESP_OK;

cleanup:
    if (d_sock >= 0)
        close(d_sock);
    if (sock >= 0)
        close(sock);
cleanup_res:
    free(res);
    return ret;
}

esp_err_t freebox_ftp_config(const freebox_ftp_config_t *config)
{
    if (!config)
        return ESP_ERR_INVALID_ARG;
    memcpy(&g_config, config, sizeof(freebox_ftp_config_t));
    return ESP_OK;
}

esp_err_t freebox_ftp_init(void) { return ESP_OK; }
void freebox_ftp_deinit(void) {}
