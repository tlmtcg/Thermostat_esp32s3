idf_component_register(
    SRCS "display_pages.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        ssd1306 
        app_context
        alert_manager
        time_utils
)