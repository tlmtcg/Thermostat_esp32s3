#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "sdkconfig.h" // Obligatoire pour lire les CONFIG_

#define MOUNT_POINT "/sdcard"

typedef struct {
    gpio_num_t mosi_io;
    gpio_num_t miso_io;
    gpio_num_t sclk_io;
    gpio_num_t cs_io;
    bool format_if_mount_failed;
} sd_card_config_t;

// Macro qui utilise maintenant les valeurs de menuconfig
#define SD_CARD_CONFIG_DEFAULT() { \
    .mosi_io = CONFIG_SD_CARD_MOSI_GPIO, \
    .miso_io = CONFIG_SD_CARD_MISO_GPIO, \
    .sclk_io = CONFIG_SD_CARD_SCLK_GPIO, \
    .cs_io   = CONFIG_SD_CARD_CS_GPIO,   \
    .format_if_mount_failed = CONFIG_SD_CARD_FORMAT_IF_FAILED \
}

esp_err_t init_sd_card(const sd_card_config_t *config);

// Fonctions utilitaires
esp_err_t sd_write_file(const char *path, const char *data);
esp_err_t sd_read_file(const char *path);
esp_err_t sd_delete_file(const char *path);
esp_err_t sd_rename_file(const char *old_path, const char *new_path);
void sd_list_files(const char *dir_path);

esp_err_t sd_create_dir(const char *path);
esp_err_t sd_remove_dir(const char *path);

void deinit_sd_card(void);

#endif