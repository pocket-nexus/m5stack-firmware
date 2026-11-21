idf_component_register(
    SRCS "main.c" "transfer_display.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app
    PRIV_REQUIRES esp_timer heap log
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME smoke
    HOST_CONFIG "../app/pocket.host.config.ts"
    AOT_CRATE "../aot")
