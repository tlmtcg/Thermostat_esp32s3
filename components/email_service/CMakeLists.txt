idf_component_register(
    SRCS "email_service.c"

    INCLUDE_DIRS "include"

    REQUIRES
        freertos
        esp-tls
        mbedtls
)

target_add_binary_data(${COMPONENT_TARGET}
    "certs/google_root.pem"
    TEXT)
    