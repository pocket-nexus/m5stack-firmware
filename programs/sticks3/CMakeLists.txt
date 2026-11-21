cmake_minimum_required(VERSION 3.22)

# StickS3 firmware: the sticks3-factory application on PocketJS (JS mode).
set(repository_root "${CMAKE_CURRENT_LIST_DIR}/../..")
set(pocketjs_components "${repository_root}/third_party/pocketjs/platforms/esp-idf")
list(APPEND EXTRA_COMPONENT_DIRS
    "${repository_root}/platforms/esp-idf/m5_bsp_sticks3"
    "${pocketjs_components}/pocket_runtime"
    "${pocketjs_components}/pocket_guest_quickjs"
    "${pocketjs_components}/pocket_display_esp_lcd"
    "${pocketjs_components}/pocket_input_gpio"
    "${pocketjs_components}/pocket_runner")
if(NOT DEFINED POCKETJS_RUST_FROM_SOURCE)
    set(POCKETJS_RUST_FROM_SOURCE ON CACHE BOOL "")
endif()

# The Pocket CLI of the pinned submodule, run with Bun. pocketjs_compile_app
# looks for POCKETJS_CLI before searching PATH for `pocket`.
if(NOT POCKETJS_CLI)
    find_program(POCKETJS_BUN bun REQUIRED)
    set(pocket_cli "${CMAKE_BINARY_DIR}/pocket-cli")
    file(WRITE "${pocket_cli}"
        "#!/bin/sh\nexec \"${POCKETJS_BUN}\" \"${repository_root}/third_party/pocketjs/tools/pocket.ts\" \"$@\"\n")
    file(CHMOD "${pocket_cli}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE)
    set(POCKETJS_CLI "${pocket_cli}" CACHE FILEPATH "Pocket CLI")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(m5_sticks3)
