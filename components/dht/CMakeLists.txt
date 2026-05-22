idf_component_register(SRCS "dht.c" "dht_task.c"
                      INCLUDE_DIRS "include"
                      PRIV_REQUIRES esp_timer esp_driver_gpio app_context sd_card time_utils)
