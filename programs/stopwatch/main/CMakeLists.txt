idf_component_register(
    SRCS "main.c" "services.c" "spectrum.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app m5_bsp_stopwatch
    PRIV_REQUIRES nvs_flash esp_timer freertos log
    EMBED_FILES "assets/boot_sfx.bin")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME stopwatch
    HOST_CONFIG "../../../apps/stopwatch-factory/pocket.host.config.ts")
