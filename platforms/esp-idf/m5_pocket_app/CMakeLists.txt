# The mode is resolved before component discovery (components.cmake) into the
# M5_POCKET_APP_MODE build property, because requirements cannot depend on
# Kconfig values: an AOT program never requires pocket_guest_quickjs.
idf_build_get_property(m5_pocket_app_mode M5_POCKET_APP_MODE)
if(NOT m5_pocket_app_mode)
    set(m5_pocket_app_mode JS)
endif()

set(m5_pocket_app_srcs "src/app.c" "src/bridge.c" "src/input.c" "src/internal_call.c" "src/profile.c"
    "src/profile_tools.c")
set(m5_pocket_app_requires pocket_runtime pocket_display_esp_lcd)
if(IDF_TARGET STREQUAL "esp32p4")
    list(APPEND m5_pocket_app_requires pocket_display_esp_dpi pocket_render_esp_ppa)
endif()
if(m5_pocket_app_mode STREQUAL "AOT")
    list(APPEND m5_pocket_app_srcs "src/app_aot.c")
else()
    list(APPEND m5_pocket_app_srcs "src/app_js.c")
    list(APPEND m5_pocket_app_requires pocket_guest_quickjs)
endif()

idf_component_register(
    SRCS ${m5_pocket_app_srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES ${m5_pocket_app_requires}
    PRIV_REQUIRES esp_timer freertos heap log esp_driver_gptimer esp_hal_usb
    REQUIRED_IDF_TARGETS esp32p4 esp32s3
)

m5_strict_warnings(${COMPONENT_LIB})

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    if(CONFIG_M5_POCKET_APP_MODE_AOT)
        set(m5_pocket_app_kconfig_mode AOT)
    else()
        set(m5_pocket_app_kconfig_mode JS)
    endif()
    if(NOT m5_pocket_app_kconfig_mode STREQUAL m5_pocket_app_mode)
        message(FATAL_ERROR "m5_pocket_app: sdkconfig selects ${m5_pocket_app_kconfig_mode} mode but "
            "components were resolved for ${m5_pocket_app_mode} mode. Include "
            "m5_pocket_app/components.cmake from the project CMakeLists.txt and run "
            "`idf.py reconfigure` after changing CONFIG_M5_POCKET_APP_MODE.")
    endif()
    if(m5_pocket_app_mode STREQUAL "AOT")
        # The AOT library calls back into the bridge; keep those members even
        # when the library follows this component on the link line.
        target_link_options(${COMPONENT_LIB} INTERFACE
            "-Wl,--undefined=m5_pocket_bridge_next_value"
            "-Wl,--undefined=m5_pocket_bridge_next_event"
            "-Wl,--undefined=m5_pocket_bridge_dispatch_request"
            "-Wl,--undefined=m5_pocket_profile_stage")
    endif()
endif()
