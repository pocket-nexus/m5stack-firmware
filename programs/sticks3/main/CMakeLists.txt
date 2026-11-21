idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES m5_bsp_sticks3 m5_sticks3_net pocket_runtime pocket_guest_quickjs
             pocket_display_esp_lcd pocket_input_gpio pocket_runner
    PRIV_REQUIRES esp_timer freertos
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

pocketjs_compile_app(
    TARGET ${COMPONENT_LIB}
    NAME sticks3_factory
    HOST_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../../apps/sticks3-factory/pocket.host.config.ts")
