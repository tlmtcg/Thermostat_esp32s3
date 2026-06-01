idf_component_register(
    SRCS "config_runtime.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash log sht31
)
