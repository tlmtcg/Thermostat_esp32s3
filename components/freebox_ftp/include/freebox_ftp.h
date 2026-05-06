#ifndef FREEBOX_FTP_H
#define FREEBOX_FTP_H

// --- Inclusions nécessaires ---
#include <stddef.h>     // Pour size_t
#include <stdbool.h>    // Pour bool
#include "esp_err.h"    // Pour esp_err_t

// --- Configuration par défaut (peut être écrasée dans sdkconfig) ---
#ifndef FREEBOX_IP
#define FREEBOX_IP       "192.168.0.254"  // IP locale de la Freebox
#endif

#ifndef FTP_PORT
#define FTP_PORT         21              // Port FTP standard (non sécurisé)
#endif

#ifndef FTP_PATH
#define FTP_PATH         "/Freebox/Logs"  // Chemin absolu sur le serveur FTP
#endif

#ifndef BUF_SIZE
#define BUF_SIZE         2048            // Taille du buffer pour les échanges FTP
#endif

// --- Tag spécifique pour les logs du composant FTP ---
static const char *FTP_TAG = "FTP_COMP";

// --- Déclarations des fonctions ---

/**
 * @brief Reçoit une réponse FTP complète depuis une socket.
 *        Attend la fin d'une ligne (\r\n) ou la fin des données.
 *
 * @param sock Socket connectée.
 * @param res Buffer pour stocker la réponse (doit être alloué par l'appelant).
 * @param max_size Taille maximale du buffer.
 * @return int Nombre d'octets reçus, ou -1 en cas d'erreur.
 */
int receive_all(int sock, char *res, size_t max_size);

/**
 * @brief Envoie une commande FTP et attend la réponse du serveur.
 *
 * @param sock Socket connectée.
 * @param cmd Commande FTP (ex: "USER", "PASS", "PASV").
 * @param arg Argument optionnel de la commande (peut être NULL).
 * @param res Buffer pour stocker la réponse (doit être alloué par l'appelant).
 * @param res_size Taille du buffer de réponse.
 * @return int Nombre d'octets reçus dans la réponse, ou -1 en cas d'erreur.
 */
int send_command(int sock, const char *cmd, const char *arg, char *res, size_t res_size);

/**
 * @brief Vérifie si une réponse FTP commence par un code donné (ex: "220", "230").
 *
 * @param res Réponse FTP reçue.
 * @param expected_code Code FTP attendu (ex: "220").
 * @return bool true si la réponse commence par le code, false sinon.
 */
bool ftp_response_starts_with(const char *res, const char *expected_code);

/**
 * @brief Upload un fichier vers un serveur FTP en mode passif.
 *
 * @param filename Nom du fichier à uploader (ex: "log.txt").
 * @param data Données à envoyer (doit être un buffer valide).
 * @param len Taille des données en octets.
 * @return esp_err_t ESP_OK en cas de succès, code d'erreur sinon.
 */
esp_err_t freebox_ftp_upload(const char *filename, const char *data, size_t len);

/**
 * @brief Télécharge un fichier depuis un serveur FTP en mode passif.
 *
 * @param filename Nom du fichier à télécharger (ex: "log.txt").
 * @param buffer Buffer pour stocker les données lues (doit être alloué par l'appelant).
 * @param buffer_size Taille maximale du buffer.
 * @param bytes_read Pointeur vers un entier pour stocker le nombre d'octets lus.
 * @return esp_err_t ESP_OK en cas de succès, code d'erreur sinon.
 */
esp_err_t freebox_ftp_download(const char *filename, char *buffer, size_t buffer_size, size_t *bytes_read);

#endif // FREEBOX_FTP_H
