#include "sd_card.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>

static const char *TAG = "SD_CARD";

// Une seule déclaration propre de la structure de la carte
static sdmmc_card_t *s_card = NULL;

/* =========================================================
   INITIALISATION
   ========================================================= */
esp_err_t init_sd_card(const sd_card_config_t *config) {
    ESP_LOGI(TAG, "Initialisation de la carte SD...");

    // 1. Configuration du bus SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_SD_CARD_MOSI_GPIO,
        .miso_io_num = CONFIG_SD_CARD_MISO_GPIO,
        .sclk_io_num = CONFIG_SD_CARD_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096, 
    };

    // Initialisation du bus avec DMA (Canal auto-alloué)
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec initialisation bus SPI (0x%x)", ret);
        return ret;
    }

    // 2. Configuration de l'hôte
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 20000; 

    // 3. Configuration du slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_SD_CARD_CS_GPIO;
    slot_config.host_id = SPI2_HOST;

    // 4. Montage avec Formatage Automatique
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true, 
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Tentative de montage (cela peut être long si formatage requis)...");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec du montage (0x%x).", ret);
        spi_bus_free(SPI2_HOST);
        s_card = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Carte SD montée et prête !");
    return ESP_OK;
}

/* =========================================================
   OPÉRATIONS SUR LES FICHIERS
   ========================================================= */
esp_err_t sd_write_file(const char *path, const char *data)
{
    ESP_LOGI(TAG, "Ouverture de %s en écriture...", path);
    FILE *f = fopen(path, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Impossible d'ouvrir le fichier %s (%s)", path, strerror(errno));
        return ESP_FAIL;
    }
    int bytes_written = fprintf(f, "%s", data);
    if (bytes_written < 0)
    {
        ESP_LOGE(TAG, "Erreur lors de l'écriture physique.");
        fclose(f);
        return ESP_FAIL;
    }
    fclose(f);
    ESP_LOGI(TAG, "Écriture réussie (%d octets).", bytes_written);
    return ESP_OK;
}

esp_err_t sd_read_file(const char *path)
{
    ESP_LOGI(TAG, "Lecture du fichier %s:", path);
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGW(TAG, "Le fichier %s n'existe pas.", path);
        return ESP_ERR_NOT_FOUND;
    }
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        printf("  > %s", line);
    }
    fclose(f);
    return ESP_OK;
}

char *sd_read_file_alloc(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        ESP_LOGW("SD", "Impossible d'ouvrir %s", path);
        return NULL;
    }

    // Aller à la fin pour connaître la taille
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        ESP_LOGW("SD", "Fichier vide : %s", path);
        return NULL;
    }

    // Allocation du buffer (+1 pour le '\0')
    char *buf = malloc(size + 1);
    if (!buf)
    {
        fclose(f);
        ESP_LOGE("SD", "malloc(%ld) a échoué", size + 1);
        return NULL;
    }

    // Lecture du fichier
    size_t read = fread(buf, 1, size, f);
    fclose(f);

    buf[read] = '\0'; // Terminaison string

    ESP_LOGI("SD", "Lecture OK (%ld octets) depuis %s", size, path);
    return buf;
}

esp_err_t sd_delete_file(const char *path)
{
    ESP_LOGI(TAG, "Tentative de suppression de %s...", path);
    if (unlink(path) != 0)
    {
        ESP_LOGE(TAG, "Erreur lors de la suppression de %s (%s)", path, strerror(errno));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Fichier supprimé.");
    return ESP_OK;
}

esp_err_t sd_rename_file(const char *old_path, const char *new_path)
{
    ESP_LOGI(TAG, "Renommage de %s vers %s", old_path, new_path);

    struct stat st;
    if (stat(new_path, &st) == 0)
    {
        ESP_LOGW(TAG, "Le fichier de destination existe déjà. Il sera écrasé.");
        unlink(new_path); 
    }

    if (rename(old_path, new_path) != 0)
    {
        ESP_LOGE(TAG, "Échec du renommage : %s", strerror(errno));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Fichier renommé avec succès.");
    return ESP_OK;
}

/* =========================================================
   RÉPERTOIRES
   ========================================================= */
void sd_list_files(const char *dir_path) {
    ESP_LOGI(TAG, "Liste des fichiers dans %s :", dir_path);

    DIR *dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire : %s", dir_path);
        return;
    }

    struct dirent *entry;
    int file_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        char full_path[320];
        struct stat st;
        
        // Gestion propre de la construction du chemin
        if (strcmp(dir_path, "/") == 0 || dir_path[strlen(dir_path) - 1] == '/') {
            snprintf(full_path, sizeof(full_path), "%s%s", dir_path, entry->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        }
        
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                ESP_LOGI(TAG, "  [DIR]  %s", entry->d_name);
            } else {
                ESP_LOGI(TAG, "  [FILE] %s (Taille: %ld octets)", entry->d_name, (long)st.st_size);
            }
            file_count++;
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Total : %d éléments trouvés.", file_count);
}

esp_err_t sd_create_dir(const char *path) {
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, path);
    if (mkdir(full_path, 0777) != 0) {
        ESP_LOGE(TAG, "Erreur création dossier %s (%s)", full_path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t sd_remove_dir(const char *path) {
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, path);
    if (rmdir(full_path) != 0) {
        ESP_LOGE(TAG, "Erreur suppression dossier %s (%s)", full_path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;          
}

/* =========================================================
   DÉINITIALISATION
   ========================================================= */
void deinit_sd_card(void)
{
    if (s_card)
    {
        ESP_LOGI(TAG, "Démontage de la carte SD...");
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
        spi_bus_free(SPI2_HOST);
        s_card = NULL;
        ESP_LOGI(TAG, "Carte SD démontée et bus libéré.");
    }
}
