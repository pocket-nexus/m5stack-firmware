idf_component_register(
    SRCS "main.c" "services.c" "spectrum.c" "badge.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app m5_bsp_stopwatch
    PRIV_REQUIRES
        nvs_flash
        esp_timer
        freertos
        log
        heap
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        fatfs
        wear_levelling
        vfs
        lwip
        espressif__esp_jpeg
    EMBED_FILES "assets/boot_sfx.bin"
    EMBED_TXTFILES "assets/badge_config_ap.html")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
m5_strict_warnings(${COMPONENT_LIB})

m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME stopwatch
    HOST_CONFIG "../../../apps/stopwatch-factory/pocket.host.config.ts")
