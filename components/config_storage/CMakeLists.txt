idf_component_register(
    SRCS "config_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES cjson vfs log sd_card task_manager
)