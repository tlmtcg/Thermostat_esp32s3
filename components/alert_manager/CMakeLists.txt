# Enregistrement du composant Alert Manager
idf_component_register(
    SRCS 
        "alert_manager.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        "led_ctrl"     # Nécessaire pour led_db_get_alarm_idx_by_name et led_db_simulate
)