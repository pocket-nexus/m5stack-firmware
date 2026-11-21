cmake_minimum_required(VERSION 3.22)

# JS mode: the application bundle runs in the QuickJS guest.
get_filename_component(repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(pocketjs_components "${repo_root}/third_party/pocketjs/platforms/esp-idf")
list(APPEND EXTRA_COMPONENT_DIRS
    "${repo_root}/platforms/esp-idf/m5_bsp_stopwatch"
    "${pocketjs_components}/pocket_runtime"
    "${pocketjs_components}/pocket_guest_quickjs"
    "${pocketjs_components}/pocket_display_esp_lcd"
    "${pocketjs_components}/pocket_input_touch"
    "${pocketjs_components}/pocket_input_gpio"
    "${pocketjs_components}/pocket_runner")

# The runtime archive is built from the submodule with Espressif's Xtensa
# Rust toolchain (rustup toolchain `esp`).
if(NOT DEFINED POCKETJS_RUST_FROM_SOURCE)
    set(POCKETJS_RUST_FROM_SOURCE ON CACHE BOOL "")
endif()
if(NOT DEFINED POCKETJS_CARGO AND EXISTS "$ENV{HOME}/.rustup/toolchains/esp/bin/cargo")
    set(POCKETJS_CARGO "$ENV{HOME}/.rustup/toolchains/esp/bin/cargo" CACHE FILEPATH "")
endif()
# The Pocket CLI runs from the submodule through Bun.
if(NOT DEFINED POCKETJS_CLI)
    set(POCKETJS_CLI "${CMAKE_CURRENT_LIST_DIR}/tools/pocket" CACHE FILEPATH "")
endif()

set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(IDF_TARGET esp32s3)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(m5stack_stopwatch)
