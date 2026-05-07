#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// --- Définition de la structure DEVANT toute utilisation ---
typedef struct {
    char host[64];          // Adresse IP ou nom d'hôte
    uint16_t port;         // Port FTP (21 par défaut)
    char user[32];         // Utilisateur FTP
    char pass[64];         // Mot de passe FTP
    char path[128];        // Chemin par défaut
    uint32_t timeout_ms;   // Timeout en ms
} freebox_ftp_config_t;


// Configuration par défaut
#ifndef FREEBOX_FTP_HOST
#define FREEBOX_FTP_HOST "192.168.0.254"
#endif

#ifndef FREEBOX_FTP_PORT
#define FREEBOX_FTP_PORT 21
#endif

#ifndef FREEBOX_FTP_USER
#define FREEBOX_FTP_USER CONFIG_FREEBOX_FTP_USER
#endif

#ifndef FREEBOX_FTP_PASS
#define FREEBOX_FTP_PASS CONFIG_FREEBOX_FTP_PASS
#endif

#ifndef FREEBOX_FTP_PATH
#define FREEBOX_FTP_PATH "/Freebox/Logs"
#endif

#ifndef FREEBOX_FTP_TIMEOUT_MS
#define FREEBOX_FTP_TIMEOUT_MS 10000
#endif

/**
 * @brief Upload un fichier vers le serveur FTP.
 * @param filename Nom du fichier à uploader.
 * @param data Données à uploader (buffer de caractères).
 * @param len Taille des données en octets.
 * @return ESP_OK en cas de succès, sinon une erreur.
 */
esp_err_t freebox_ftp_upload(const char *filename, const char *data, size_t len);

/**
 * @brief Télécharge un fichier depuis le serveur FTP.
 * @param filename Nom du fichier à télécharger.
 * @param buffer Buffer pour stocker les données (doit être alloué par l'appelant).
 * @param buffer_size Taille maximale du buffer.
 * @param bytes_read Pointeur vers un entier pour stocker le nombre d'octets lus.
 * @return ESP_OK en cas de succès, sinon une erreur.
 */
esp_err_t freebox_ftp_download(const char *filename, char *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * @brief Récupère la liste des fichiers du répertoire courant.
 * @param buffer Buffer pour stocker le texte de la liste.
 * @param buffer_size Taille du buffer.
 */
esp_err_t freebox_ftp_list(char *buffer, size_t buffer_size);

/**
 * @brief Supprime un fichier sur le serveur.
 */
esp_err_t freebox_ftp_delete(const char *filename);

/**
 * @brief Renomme un fichier sur le serveur.
 */
esp_err_t freebox_ftp_rename(const char *old_name, const char *new_name);

/**
 * @brief Modifie un fichier (Download -> Callback de modification -> Upload).
 * @param filename Nom du fichier à éditer.
 * @param edit_callback Fonction prenant le buffer et sa taille pour modification.
 */
esp_err_t freebox_ftp_edit(const char *filename, void (*edit_callback)(char *buffer, size_t *len));

/**
 * @brief Libère les ressources du client.
 */
void freebox_ftp_deinit(void);
