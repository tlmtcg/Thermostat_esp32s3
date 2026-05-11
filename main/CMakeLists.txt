if(!CONFIG_USE_APP_MAIN)
    set(my_srcs "main_app.c")
    # Liste complète pour l'appli réelle
    set(my_requires wifi web_server time_utils led_ctrl alert_manager 
                    task_manager serial_manager freebox_ftp 
                    heating_program relay sd_card email_service)
else()
    set(my_srcs "main_app_test.c")
    # Liste réduite au strict minimum pour le test d'email
    set(my_requires wifi email_service) 
endif()

idf_component_register(
    SRCS "${my_srcs}"
    INCLUDE_DIRS "." 
    PRIV_REQUIRES esp_http_server joltwallet__littlefs
    REQUIRES "${my_requires}"
)