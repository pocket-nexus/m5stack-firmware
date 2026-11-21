cmake_minimum_required(VERSION 3.22)

# StickS3 firmware: the sticks3-demos application through m5_pocket_app
# (JS mode by default, CONFIG_M5_POCKET_APP_MODE_*).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_pocket_app/components.cmake)
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_bsp_sticks3")
idf_build_set_property(MINIMAL_BUILD ON)
# The two modes resolve different managed components (AOT has no QuickJS);
# each keeps its own lock file.
if(M5_POCKET_APP_MODE STREQUAL "AOT")
    idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_CURRENT_LIST_DIR}/dependencies.aot.lock")
endif()
set(PROJECT_VER "0.2.0")
project(m5_sticks3)
