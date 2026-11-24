cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_pocket_app/components.cmake)
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_bsp_corep4x")
# AOT mode links no QuickJS, so its managed components differ; it keeps its
# own lock file.
if(SDKCONFIG_DEFAULTS MATCHES "sdkconfig\\.aot")
    idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_CURRENT_LIST_DIR}/dependencies.aot.lock")
endif()
idf_build_set_property(MINIMAL_BUILD ON)
project(m5_corep4x_factory)
