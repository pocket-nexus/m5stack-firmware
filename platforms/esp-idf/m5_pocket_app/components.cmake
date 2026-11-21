# Component discovery for a program that embeds its application through
# m5_pocket_app. Include it from the project CMakeLists.txt after IDF's
# project.cmake and before project():
#
#   cmake_minimum_required(VERSION 3.22)
#   include($ENV{IDF_PATH}/tools/cmake/project.cmake)
#   include(${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_pocket_app/components.cmake)
#   project(<name>)
#
# It reads CONFIG_M5_POCKET_APP_MODE_* from sdkconfig, or from the
# sdkconfig.defaults files when sdkconfig does not exist yet, because IDF
# resolves component requirements before it loads Kconfig values. JS mode adds
# the QuickJS guest; AOT mode never adds it, and replaces the runtime archive
# of pocket_runtime with the application library m5_pocket_app() builds.
include_guard(GLOBAL)

get_filename_component(M5_POCKET_APP_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(M5_POCKET_REPO_DIR "${M5_POCKET_APP_DIR}/../../.." ABSOLUTE)
set(M5_POCKET_POCKETJS_DIR "${M5_POCKET_REPO_DIR}/third_party/pocketjs")

function(_m5_pocket_app_mode_from file out)
    if(NOT EXISTS "${file}")
        return()
    endif()
    file(STRINGS "${file}" lines REGEX "^(# )?CONFIG_M5_POCKET_APP_MODE_(JS|AOT)")
    foreach(line IN LISTS lines)
        if(line MATCHES "^CONFIG_M5_POCKET_APP_MODE_(JS|AOT)=y")
            set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        elseif(line MATCHES "^# CONFIG_M5_POCKET_APP_MODE_AOT is not set" AND NOT ${out})
            set(${out} JS PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

function(_m5_pocket_app_resolve_mode out)
    set(target "${IDF_TARGET}")
    if(NOT target AND DEFINED ENV{IDF_TARGET})
        set(target "$ENV{IDF_TARGET}")
    endif()
    if(SDKCONFIG)
        get_filename_component(sdkconfig "${SDKCONFIG}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    else()
        set(sdkconfig "${CMAKE_SOURCE_DIR}/sdkconfig")
    endif()
    set(mode "")
    if(EXISTS "${sdkconfig}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${sdkconfig}")
        _m5_pocket_app_mode_from("${sdkconfig}" mode)
    else()
        if(SDKCONFIG_DEFAULTS)
            set(defaults ${SDKCONFIG_DEFAULTS})
        elseif(DEFINED ENV{SDKCONFIG_DEFAULTS})
            set(defaults $ENV{SDKCONFIG_DEFAULTS})
        else()
            set(defaults sdkconfig.defaults)
        endif()
        foreach(file IN LISTS defaults)
            get_filename_component(file "${file}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
            # Later files override earlier ones, as in IDF's own merge.
            _m5_pocket_app_mode_from("${file}" mode)
            if(target)
                _m5_pocket_app_mode_from("${file}.${target}" mode)
            endif()
        endforeach()
    endif()
    if(NOT mode)
        set(mode JS)
    endif()
    set(${out} "${mode}" PARENT_SCOPE)
endfunction()

_m5_pocket_app_resolve_mode(M5_POCKET_APP_MODE)
idf_build_set_property(M5_POCKET_APP_MODE "${M5_POCKET_APP_MODE}")
message(STATUS "m5_pocket_app: ${M5_POCKET_APP_MODE} mode")

set(m5_pocket_target "${IDF_TARGET}")
if(NOT m5_pocket_target AND DEFINED ENV{IDF_TARGET})
    set(m5_pocket_target "$ENV{IDF_TARGET}")
endif()

set(m5_pocket_components "${M5_POCKET_POCKETJS_DIR}/platforms/esp-idf")
list(APPEND EXTRA_COMPONENT_DIRS
    "${M5_POCKET_APP_DIR}"
    "${m5_pocket_components}/pocket_runtime"
    "${m5_pocket_components}/pocket_display_esp_lcd"
    "${m5_pocket_components}/pocket_input_gpio"
    "${m5_pocket_components}/pocket_input_touch")
if(m5_pocket_target STREQUAL "esp32p4")
    list(APPEND EXTRA_COMPONENT_DIRS
        "${m5_pocket_components}/pocket_display_esp_dpi"
        "${m5_pocket_components}/pocket_render_esp_ppa")
endif()

if(NOT DEFINED POCKETJS_CLI)
    set(POCKETJS_CLI "${M5_POCKET_REPO_DIR}/tools/pocket" CACHE FILEPATH "Pocket CLI")
endif()

if(M5_POCKET_APP_MODE STREQUAL "AOT")
    # pocket_runtime links the application library in place of its default
    # archive; its hooks (pocket_host_alloc, ...) serve the library's
    # allocator and panic handler. m5_pocket_app() builds the library to this
    # path; the placeholder is an empty archive until the first build.
    set(M5_POCKET_AOT_ARCHIVE "${CMAKE_BINARY_DIR}/m5_pocket_aot/libm5_pocket_app_aot.a")
    if(NOT EXISTS "${M5_POCKET_AOT_ARCHIVE}")
        file(WRITE "${M5_POCKET_AOT_ARCHIVE}" "!<arch>\n")
    endif()
    set(POCKETJS_RUST_FROM_SOURCE OFF)
    set(POCKET_RUNTIME_ARCHIVE "${M5_POCKET_AOT_ARCHIVE}")
else()
    list(APPEND EXTRA_COMPONENT_DIRS "${m5_pocket_components}/pocket_guest_quickjs")
    # Without a Registry archive the runtime archive is built from source:
    # ESP32-S3 with Espressif's Xtensa toolchain, ESP32-P4 with the toolchain
    # third_party/pocketjs/rust-toolchain.toml pins.
    if(NOT DEFINED POCKETJS_RUST_FROM_SOURCE AND NOT POCKET_RUNTIME_ARCHIVE)
        set(POCKETJS_RUST_FROM_SOURCE ON CACHE BOOL "Build the PocketJS runtime archive from source")
    endif()
    if(NOT DEFINED POCKETJS_CARGO AND m5_pocket_target STREQUAL "esp32s3"
       AND EXISTS "$ENV{HOME}/.rustup/toolchains/esp/bin/cargo")
        set(POCKETJS_CARGO "$ENV{HOME}/.rustup/toolchains/esp/bin/cargo" CACHE FILEPATH "Cargo of the Xtensa toolchain")
    endif()
endif()
unset(m5_pocket_components)
unset(m5_pocket_target)
