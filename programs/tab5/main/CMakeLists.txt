idf_component_register(
    SRCS "main.c" "services.c" "sound.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app m5_bsp_tab5
    PRIV_REQUIRES
        esp_driver_dma
        esp_driver_gpio
        esp_lcd
        esp_mm
        esp_timer
        fatfs
        heap
        log
        vfs
    EMBED_FILES
        "../audio/startup_sfx.mp3"
        "../audio/shutdown_sfx.mp3"
        "../audio/canon_in_d.mp3"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

# The application and its host config live in apps/tab5-factory; the host
# contract is this program's pocket.host.json.
m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME tab5
    HOST_CONFIG "../../../apps/tab5-factory/pocket.host.config.ts")
