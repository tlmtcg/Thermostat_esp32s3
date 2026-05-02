#include "ws_api_led.h"
#include "led_service.h"
#include "led_db.h"    // Pour led_db_exists, add_info, add_alarm
#include "led_types.h" // Pour led_color_t
#include "led_ctrl.h"  // Pour led_stop()
#include "esp_log.h"
#include "cJSON.h"

cJSON* led_db_get_json_status(void) 
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    // On crée les tableaux et on stocke les pointeurs dans 'infos' et 'alarms'
    cJSON *infos = cJSON_AddArrayToObject(root, "infos");
    cJSON *alarms = cJSON_AddArrayToObject(root, "alarms");

    if (!infos || !alarms) {
        cJSON_Delete(root);
        return NULL;
    }

    // Section INFOS
    for (int i = 0; i < led_db_get_info_count(); i++) {
        stored_info_t *item = led_db_get_info_by_idx(i);
        if (item) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "id", i); // Ajout de l'ID pour le JS
            cJSON_AddStringToObject(obj, "name", item->name);
            cJSON_AddNumberToObject(obj, "r", item->color.r);
            cJSON_AddNumberToObject(obj, "g", item->color.g);
            cJSON_AddNumberToObject(obj, "b", item->color.b);
            cJSON_AddItemToArray(infos, obj); // Utilise bien 'infos'
        }
    }

    // Section ALARMES
    for (int i = 0; i < led_db_get_alarm_count(); i++) {
        stored_alarm_t *item = led_db_get_alarm_by_idx(i);
        if (item) {
            cJSON *alarm_obj = cJSON_CreateObject(); // Renommé pour éviter confusion
            cJSON_AddNumberToObject(alarm_obj, "id", i);
            cJSON_AddStringToObject(alarm_obj, "name", item->name);
            cJSON_AddNumberToObject(alarm_obj, "blinks", item->blinks);
            cJSON_AddNumberToObject(alarm_obj, "r", item->color.r);
            cJSON_AddNumberToObject(alarm_obj, "g", item->color.g);
            cJSON_AddNumberToObject(alarm_obj, "b", item->color.b);
            cJSON_AddItemToArray(alarms, alarm_obj); // Utilise bien 'alarms'
        }
    }

    return root;
}