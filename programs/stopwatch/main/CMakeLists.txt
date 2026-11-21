idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES m5_bsp_stopwatch pocket_runtime pocket_guest_quickjs
             pocket_display_esp_lcd pocket_input_touch pocket_input_gpio
             pocket_runner
    PRIV_REQUIRES nvs_flash esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

get_filename_component(stopwatch_app "${CMAKE_CURRENT_LIST_DIR}/../../../apps/stopwatch-factory" ABSOLUTE)
pocketjs_compile_app(
    TARGET ${COMPONENT_LIB}
    NAME stopwatch
    HOST_CONFIG "${stopwatch_app}/pocket.host.config.ts")
