cmake_minimum_required(VERSION 3.22)

# The application mode (JS by default, AOT with CONFIG_M5_POCKET_APP_MODE_AOT)
# and the PocketJS components come from the shared application host.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_pocket_app/components.cmake)
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_bsp_stopwatch")
set(IDF_TARGET esp32s3)
# The modes need different managed components (AOT links no QuickJS), so
# each keeps its own lock file.
if(M5_POCKET_APP_MODE STREQUAL "AOT")
    idf_build_set_property(DEPENDENCIES_LOCK
        "${CMAKE_CURRENT_LIST_DIR}/dependencies.aot.lock")
endif()
idf_build_set_property(MINIMAL_BUILD ON)
project(m5stack_stopwatch)
