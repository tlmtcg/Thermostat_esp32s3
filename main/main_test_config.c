#include "esp_log.h"
#include "cJSON.h"

// Tes composants
#include "sd_card.h"
#include "config_storage.h"
#include "i2c_manager.h"
#include "task_manager.h"

static const char *TAG = "TEST_APP";

#define CONFIG_FILE MOUNT_POINT "/config.json"

void app_main(void)
{

    // 2) Charger la config JSON si elle existe
    cJSON *config_json = load_json_from_sdcard(CONFIG_FILE);

    if (config_json) {
        ESP_LOGI(TAG, "Configuration chargée depuis %s", CONFIG_FILE);

        // TODO : appliquer la config JSON (I2C, SD, etc.)
        // apply_config_json(config_json);

        cJSON_Delete(config_json);
    } else {
        ESP_LOGW(TAG, "Aucune config JSON → génération depuis Kconfig");

        // 3) Sauvegarder la config Kconfig → JSON
        save_kconfig_to_sdcard(CONFIG_FILE);
    }

    // 4) Initialiser le bus I2C
    i2c_manager_init();

    task_manager_init();    

}

