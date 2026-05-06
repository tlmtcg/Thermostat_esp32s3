#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include "lwip/sockets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "FTP_COMP";

// --- CONFIGURATION LOCALE (à adapter) ---
#define FREEBOX_IP       "192.168.0.254"  // IP LOCALE de la Freebox (remplace par la tienne)
#define FTP_USER         CONFIG_FREEBOX_FTP_USER
#define FTP_PASS         CONFIG_FREEBOX_FTP_PASSWORD
#define FTP_PORT         21              // Port FTP local (non sécurisé)
#define FTP_PATH         "/Freebox/Logs"  // Chemin absolu (doit exister)
#define BUF_SIZE         1024

// --- Fonction pour recevoir une réponse FTP complète ---
static int receive_all(int sock, char *res, size_t max_size) {
    memset(res, 0, max_size);
    int total_bytes = 0;
    int bytes;
    char *ptr = res;

    while (1) {
        bytes = recv(sock, ptr, max_size - total_bytes - 1, 0);
        if (bytes <= 0) break;
        total_bytes += bytes;
        ptr += bytes;
        // On s'arrête à la fin de la ligne (\r\n)
        if (strstr(res, "\r\n") != NULL) break;
    }
    return total_bytes;
}

// --- Fonction pour envoyer une commande FTP ---
static int send_command(int sock, const char *cmd, const char *arg, char *res, size_t res_size) {
    char buffer[BUF_SIZE];
    int len;
    if (arg) len = snprintf(buffer, sizeof(buffer), "%s %s\r\n", cmd, arg);
    else len = snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);

    ESP_LOGI(TAG, "> %s", buffer);  // Log de la commande envoyée
    if (send(sock, buffer, len, 0) < 0) {
        ESP_LOGE(TAG, "Erreur envoi commande: %s", cmd);
        return -1;
    }

    int received = receive_all(sock, res, res_size);
    ESP_LOGI(TAG, "< %s", res);  // Log de la réponse du serveur
    return received;
}

esp_err_t freebox_ftp_upload(const char *filename, const char *data, size_t len)
{
    if (!filename || !data || len == 0) {
        ESP_LOGE(TAG, "Paramètres invalides");
        return ESP_ERR_INVALID_ARG;
    }

    char res[BUF_SIZE] = {0};
    char cmd[BUF_SIZE] = {0};
    int sock = -1, data_sock = -1;
    esp_err_t ret = ESP_FAIL;

    struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};

    // --- 1. Socket commande ---
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed");
        goto cleanup;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(FTP_PORT),
        .sin_addr.s_addr = inet_addr(FREEBOX_IP),
    };

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Connexion échouée");
        goto cleanup;
    }

    if (receive_all(sock, res, sizeof(res)) <= 0 || strstr(res, "220") == NULL) {
        ESP_LOGE(TAG, "Pas de welcome FTP");
        goto cleanup;
    }

    // --- 2. Auth ---
    if (send_command(sock, "USER", FTP_USER, res, sizeof(res)) < 0) goto cleanup;
    if (send_command(sock, "PASS", FTP_PASS, res, sizeof(res)) < 0) goto cleanup;

    if (strstr(res, "230") == NULL) {
        ESP_LOGE(TAG, "Auth failed: %s", res);
        goto cleanup;
    }

    // --- 3. CWD sécurisé ---
    char normalized_path[BUF_SIZE] = {0};
    size_t path_len = strlen(FTP_PATH);

    if (path_len >= sizeof(normalized_path) - 2) {
        ESP_LOGE(TAG, "FTP_PATH trop long");
        goto cleanup;
    }

    snprintf(normalized_path, sizeof(normalized_path), "%s%s",
             FTP_PATH,
             (FTP_PATH[path_len - 1] == '/') ? "" : "/");

    snprintf(cmd, sizeof(cmd), "CWD %.*s",
             (int)(sizeof(cmd) - 5), normalized_path);

    if (send_command(sock, cmd, NULL, res, sizeof(res)) < 0 ||
        strstr(res, "250") == NULL) {
        ESP_LOGE(TAG, "CWD failed: %s", res);
        goto cleanup;
    }

    // --- 4. Mode binaire ---
    if (send_command(sock, "TYPE", "I", res, sizeof(res)) < 0 ||
        strstr(res, "200") == NULL) {
        ESP_LOGE(TAG, "TYPE failed");
        goto cleanup;
    }

    // --- 5. PASV ---
    if (send_command(sock, "PASV", NULL, res, sizeof(res)) < 0) {
        ESP_LOGE(TAG, "PASV failed");
        goto cleanup;
    }

    int a, b, c, d, p1, p2;
    char *p = strchr(res, '(');
    if (!p || sscanf(p + 1, "%d,%d,%d,%d,%d,%d",
                     &a, &b, &c, &d, &p1, &p2) != 6) {
        ESP_LOGE(TAG, "PASV parse error: %s", res);
        goto cleanup;
    }

    char ip[16];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", a, b, c, d);
    int port = (p1 << 8) | p2;

    // --- 6. Socket DATA ---
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) {
        ESP_LOGE(TAG, "data socket failed");
        goto cleanup;
    }

    setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in data_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip),
    };

    if (connect(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) != 0) {
        ESP_LOGE(TAG, "DATA connect failed");
        goto cleanup;
    }

    // --- 7. STOR ---
    if (send_command(sock, "STOR", filename, res, sizeof(res)) < 0 ||
        (strstr(res, "150") == NULL && strstr(res, "125") == NULL)) {
        ESP_LOGE(TAG, "STOR refused: %s", res);
        goto cleanup;
    }

    // --- 8. Envoi robuste ---
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(data_sock, data + total, len - total, 0);

        if (sent < 0) {
            ESP_LOGE(TAG, "send error");
            goto cleanup;
        }

        if (sent == 0) {
            ESP_LOGE(TAG, "connexion fermée prématurément");
            goto cleanup;
        }

        total += sent;
    }

    shutdown(data_sock, SHUT_WR); // important

    // --- 9. Fin transfert ---
    if (receive_all(sock, res, sizeof(res)) <= 0 ||
        strstr(res, "226") == NULL) {
        ESP_LOGE(TAG, "Pas de confirmation 226: %s", res);
        goto cleanup;
    }

    ESP_LOGI(TAG, "Upload OK (%d bytes)", (int)total);
    ret = ESP_OK;

cleanup:
    if (data_sock >= 0) close(data_sock);
    if (sock >= 0) close(sock);
    return ret;
}


/**
 * @brief Télécharge un fichier depuis le serveur FTP de la Freebox.
 * @param filename Nom du fichier à télécharger (ex: "test_esp32.txt").
 * @param buffer Buffer pour stocker les données lues (doit être alloué par l'appelant).
 * @param buffer_size Taille maximale du buffer.
 * @param bytes_read Pointeur vers un entier pour stocker le nombre d'octets lus.
 * @return ESP_OK en cas de succès, ESP_FAIL sinon.
 */
esp_err_t freebox_ftp_download(const char *filename, char *buffer, size_t buffer_size, size_t *bytes_read) {
    char res[BUF_SIZE];
    int sock = -1, data_sock = -1;
    *bytes_read = 0;  // Initialise à 0

    // --- 1. Création de la socket (IPv4) ---
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Erreur creation socket");
        return ESP_FAIL;
    }

    // Timeout
    struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Adresse du serveur FTP
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(FTP_PORT),
        .sin_addr.s_addr = inet_addr(FREEBOX_IP),
    };

    ESP_LOGI(TAG, "Connexion a %s:%d...", FREEBOX_IP, FTP_PORT);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Erreur connexion");
        close(sock);
        return ESP_FAIL;
    }

    // --- 2. Attend la bannière (220) ---
    if (receive_all(sock, res, BUF_SIZE) <= 0 || strstr(res, "220") == NULL) {
        ESP_LOGE(TAG, "Pas de reponse du serveur (attendu 220)");
        close(sock);
        return ESP_FAIL;
    }

    // --- 3. Authentification ---
    if (send_command(sock, "USER", FTP_USER, res, BUF_SIZE) < 0 || strstr(res, "331") == NULL) {
        ESP_LOGE(TAG, "Erreur USER (attendu 331)");
        close(sock);
        return ESP_FAIL;
    }

    if (send_command(sock, "PASS", FTP_PASS, res, BUF_SIZE) < 0 || strstr(res, "230") == NULL) {
        ESP_LOGE(TAG, "Authentification echouee (attendu 230)");
        close(sock);
        return ESP_FAIL;
    }

    // --- 4. Changement de répertoire ---
    char cwd_cmd[64];
    char normalized_path[64];
    snprintf(normalized_path, sizeof(normalized_path), "%s%s",
             FTP_PATH, (FTP_PATH[strlen(FTP_PATH) - 1] == '/') ? "" : "/");
    snprintf(cwd_cmd, sizeof(cwd_cmd), "CWD %.*s",
         (int)(sizeof(cwd_cmd) - 5), normalized_path);

    if (send_command(sock, cwd_cmd, NULL, res, BUF_SIZE) < 0 || strstr(res, "250") == NULL) {
        ESP_LOGE(TAG, "Erreur CWD (attendu 250). Chemin: %s", normalized_path);
        close(sock);
        return ESP_FAIL;
    }

    // --- 5. Mode binaire (TYPE I) ---
    if (send_command(sock, "TYPE", "I", res, BUF_SIZE) < 0 || strstr(res, "200") == NULL) {
        ESP_LOGE(TAG, "Erreur TYPE I (attendu 200)");
        close(sock);
        return ESP_FAIL;
    }

    // --- 6. Mode passif (PASV) ---
    if (send_command(sock, "PASV", NULL, res, BUF_SIZE) < 0 || strstr(res, "227") == NULL) {
        ESP_LOGE(TAG, "Mode PASV indisponible (attendu 227)");
        close(sock);
        return ESP_FAIL;
    }

    // --- 7. Parsing de PASV (format: (a,b,c,d,p1,p2)) ---
    char *start_ptr = strchr(res, '(');
    char *end_ptr = strchr(res, ')');
    if (!start_ptr || !end_ptr) {
        ESP_LOGE(TAG, "Format PASV invalide: %s", res);
        close(sock);
        return ESP_FAIL;
    }

    int a, b, c, d, p1, p2;
    if (sscanf(start_ptr + 1, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &p1, &p2) != 6) {
        ESP_LOGE(TAG, "Echec parsing PASV: %s", res);
        close(sock);
        return ESP_FAIL;
    }

    char pasv_ip[16];
    snprintf(pasv_ip, sizeof(pasv_ip), "%d.%d.%d.%d", a, b, c, d);
    int data_port = (p1 << 8) | p2;

    ESP_LOGI(TAG, "Connexion de donnees: IP=%s, Port=%d", pasv_ip, data_port);

    // --- 8. Connexion au canal de données ---
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) {
        ESP_LOGE(TAG, "Erreur creation socket de donnees");
        close(sock);
        return ESP_FAIL;
    }

    struct sockaddr_in data_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(data_port),
        .sin_addr.s_addr = inet_addr(pasv_ip),
    };

    if (connect(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) != 0) {
        ESP_LOGE(TAG, "Echec connexion canal DATA (IP: %s, Port: %d)", pasv_ip, data_port);
        close(data_sock);
        close(sock);
        return ESP_FAIL;
    }

    // --- 9. Demande de téléchargement (RETR) ---
    if (send_command(sock, "RETR", filename, res, BUF_SIZE) < 0 ||
        (strstr(res, "150") == NULL && strstr(res, "125") == NULL)) {
        ESP_LOGE(TAG, "Serveur non pret pour RETR (attendu 150/125)");
        close(data_sock);
        close(sock);
        return ESP_FAIL;
    }

    // --- 10. Lecture des données ---
    ssize_t total_received = 0;
    ssize_t received;
    while (total_received < buffer_size - 1) {  // -1 pour éviter le débordement
        received = recv(data_sock, buffer + total_received, buffer_size - total_received - 1, 0);
        if (received <= 0) break;  // Fin de la réception ou erreur
        total_received += received;
    }
    buffer[total_received] = '\0';  // Termine la chaîne (optionnel si binaire)
    *bytes_read = total_received;

    ESP_LOGI(TAG, "Donnees recues (%d octets)", total_received);

    // --- 11. Attend la confirmation (226) ---
    if (receive_all(sock, res, BUF_SIZE) <= 0 || strstr(res, "226") == NULL) {
        ESP_LOGE(TAG, "Transfert echoue (attendu 226): %s", res);
        close(data_sock);
        close(sock);
        return ESP_FAIL;
    }

    // --- 12. Nettoyage ---
    close(data_sock);
    close(sock);
    ESP_LOGI(TAG, "Fichier %s telecharge avec succes (%d octets).", filename, total_received);
    return ESP_OK;
}

