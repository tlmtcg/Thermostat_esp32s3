// Dans oled_service.h
#pragma once

#include "ssd1306.h"
#include "sht31.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- Constants ---
#define TEMP_HISTORY_SIZE 30  // Nombre de points dans l'historique
#define GRAPH_WIDTH       128 // Largeur de l'OLED (SSD1306)
#define GRAPH_HEIGHT      64  // Hauteur de l'OLED
#define GRAPH_Y_OFFSET    48  // Position Y du graphique (après le texte)
#define GRAPH_X_START     0   // Position X de départ
#define TEMP_MIN          15.0f // Température minimale pour l'échelle
#define TEMP_MAX          35.0f // Température maximale pour l'échelle

// --- Structs ---
typedef struct {
    float temp_history[TEMP_HISTORY_SIZE];
    uint8_t history_index;
    bool history_full;
} oled_graph_data_t;

// --- Public Functions ---
esp_err_t oled_service_init(i2c_master_bus_handle_t bus);
void oled_service_show_boot(void);
void oled_service_show_error(const char *msg);
void oled_service_show_text(const char *line1, const char *line2, const char *line3);
void oled_service_show_temp_hum(float temp, float hum);
void oled_service_add_temp_to_history(float temp); // Nouvelle fonction