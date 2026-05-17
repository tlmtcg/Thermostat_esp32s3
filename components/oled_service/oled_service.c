#include "oled_service.h"
#include "ssd1306.h"
#include "sht31.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time_utils.h"
#include "font3x5.h"
#include "font5x7.h"

// --- Constants ---
static const char *TAG = "OLED_SERVICE";
#define OLED_I2C_ADDR         0x3C
#define OLED_TASK_STACK_SIZE  4096
#define OLED_TASK_PRIORITY    5
#define OLED_TASK_DELAY_MS    2000
#define OLED_LINE_BUFFER_SIZE 32
#define OLED_Y_POS_TITLE       0
#define OLED_Y_POS_LINE1       16
#define OLED_Y_POS_LINE2       32

// --- Global OLED Instance ---

static oled_graph_data_t graph_data = {0};

// --- Helper Macros ---
#define OLED_CLEAR_AND_UPDATE() \
    do {                        \
        ssd1306_clear(&oled);   \
        ssd1306_update(&oled);  \
    } while(0)

// --- Helper Functions (Static) ---
static void oled_draw_error(const char *msg) {
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, OLED_Y_POS_TITLE, "ERROR");
    if (msg) {
        ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE1, msg);
    }
    ssd1306_update(&oled);
}

// Fonction pour ajouter une température à l'historique
void oled_service_add_temp_to_history(float temp) {
    graph_data.temp_history[graph_data.history_index] = temp;
    graph_data.history_index = (graph_data.history_index + 1) % TEMP_HISTORY_SIZE;
    if (graph_data.history_index == 0) {
        graph_data.history_full = true;
    }
}

// Fonction pour dessiner le graphique
static void oled_draw_temp_graph(void) {
    if (!graph_data.history_full && graph_data.history_index == 0) {
        return; // Pas assez de données
    }

    // Calculer le nombre de points à afficher
    uint8_t points_to_draw = graph_data.history_full ? TEMP_HISTORY_SIZE : graph_data.history_index;

    // Dessiner les axes (optionnel)
    ssd1306_draw_line(&oled, GRAPH_X_START, GRAPH_Y_OFFSET, GRAPH_X_START + GRAPH_WIDTH - 1, GRAPH_Y_OFFSET,1); // Axe X
    ssd1306_draw_line(&oled, GRAPH_X_START, GRAPH_Y_OFFSET - GRAPH_HEIGHT, GRAPH_X_START, GRAPH_Y_OFFSET,1); // Axe Y

    // Dessiner les points et les lignes
    uint8_t prev_x = GRAPH_X_START;
    uint8_t prev_y = GRAPH_Y_OFFSET - (uint8_t)((graph_data.temp_history[0] - TEMP_MIN) / (TEMP_MAX - TEMP_MIN) * GRAPH_HEIGHT);

    for (uint8_t i = 1; i < points_to_draw; i++) {
        uint8_t current_index = (graph_data.history_index - i + TEMP_HISTORY_SIZE) % TEMP_HISTORY_SIZE;
        float temp = graph_data.temp_history[current_index];

        // Normaliser la température entre 0 et GRAPH_HEIGHT
        uint8_t current_y = GRAPH_Y_OFFSET - (uint8_t)((temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN) * GRAPH_HEIGHT);
        uint8_t current_x = GRAPH_X_START + (i * (GRAPH_WIDTH - 1) / (TEMP_HISTORY_SIZE - 1));

        // Dessiner une ligne entre les points
        ssd1306_draw_line(&oled, prev_x, prev_y, current_x, current_y,1);

        // Mettre à jour les coordonnées précédentes
        prev_x = current_x;
        prev_y = current_y;
    }
}

// --- OLED Task ---
static void oled_task(void *arg) {
    char line[OLED_LINE_BUFFER_SIZE];
    char hhmmss[16];

    while (1) {
        ssd1306_clear(&oled);

        // Draw title (time)
        time_utils_get_hour_str(hhmmss, sizeof(hhmmss));
        ssd1306_draw_string(&oled, 0, OLED_Y_POS_TITLE, hhmmss);

        // Draw SHT31 data
        const sht31_state_t *state = sht31_get_state();
        if (state && state->valid) {
            snprintf(line, sizeof(line), "TEMP: %.1f C", state->temperature);
            ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE1, line);

            snprintf(line, sizeof(line), "HUM: %.1f %%", state->humidity);
            ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE2, line);

            // Ajouter la température à l'historique
            oled_service_add_temp_to_history(state->temperature);
        } else {
            ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE1, "SHT31 ERROR");
        }

        // Dessiner le graphique
        oled_draw_temp_graph();

        // Update display
        esp_err_t err = ssd1306_update(&oled);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OLED update failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_DELAY_MS));
    }
}


// --- Public Functions ---
esp_err_t oled_service_init(i2c_master_bus_handle_t bus) {
    configASSERT(bus != NULL && "I2C bus handle cannot be NULL");

    ESP_LOGI(TAG, "Initializing OLED...");

    esp_err_t err = ssd1306_init(&oled, bus, OLED_I2C_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ssd1306_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Initialiser l'historique
    graph_data.history_index = 0;
    graph_data.history_full = false;
    for (uint8_t i = 0; i < TEMP_HISTORY_SIZE; i++) {
        graph_data.temp_history[i] = TEMP_MIN; // Initialiser à la température minimale
    }

    // Show boot screen
    oled_service_show_boot();

    // Start OLED task
    // BaseType_t res = xTaskCreate(
    //     oled_task,
    //     "oled_task",
    //     OLED_TASK_STACK_SIZE,
    //     NULL,
    //     OLED_TASK_PRIORITY,
    //     NULL
    // );
    // if (res != pdPASS) {
    //     ESP_LOGE(TAG, "Failed to create OLED task");
    //     return ESP_FAIL;
    // }

    return ESP_OK;
}

void oled_service_show_boot(void) {
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, OLED_Y_POS_TITLE, "THERMOSTAT");
    ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE1, "Starting...");
    ssd1306_update(&oled);
}

void oled_service_show_error(const char *msg) {
    oled_draw_error(msg);
}

void oled_service_show_text(const char *line1, const char *line2, const char *line3) {
    ssd1306_clear(&oled);

    if (line1) ssd1306_draw_string(&oled, 0, OLED_Y_POS_TITLE, line1);
    if (line2) ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE1, line2);
    if (line3) ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE2, line3);

    ssd1306_update(&oled);
}

void oled_service_show_temp_hum(float temp, float hum) {
    char line1[OLED_LINE_BUFFER_SIZE];
    char line2[OLED_LINE_BUFFER_SIZE];

    snprintf(line1, sizeof(line1), "Temp: %.1f C", temp);
    snprintf(line2, sizeof(line2), "Hum: %.1f %%", hum);

    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, OLED_Y_POS_TITLE, "THERMOSTAT");
    ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE1, line1);
    ssd1306_draw_string(&oled, 0, OLED_Y_POS_LINE2, line2);

    // Ajouter la température à l'historique
    oled_service_add_temp_to_history(temp);

    // Dessiner le graphique
    oled_draw_temp_graph();

    ssd1306_update(&oled);
}
