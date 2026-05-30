#include "sd_card.h"

void test_sd_card(void) {
    // 1. Initialisation (utilise les défauts du Kconfig)
    if (init_sd_card(NULL) != ESP_OK) return;

    // Lister le contenu de la racine
    sd_list_files(MOUNT_POINT);

    const char* file_path = MOUNT_POINT"/config.txt";
    const char* new_path = MOUNT_POINT"/old_config.txt";

    // 2. Écrire
    sd_write_file(file_path, "Temperature: 22.5\nStatus: OK","a");

    // 3. Lire
    sd_read_file(file_path);

    // 4. Renommer
    sd_rename_file(file_path, new_path);

    // 5. Supprimer
    // sd_delete_file(new_path);
}

