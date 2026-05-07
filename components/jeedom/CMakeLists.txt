idf_component_register(SRCS "jeedom.c" 
                    INCLUDE_DIRS "include"
                    REQUIRES
                        weather
                    REQUIRES 
                        cjson
                        esp_http_client
                        esp_http_server
                        esp_timer
                        esp_wifi
                        time_utils
                    )
                    