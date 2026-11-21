cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_pocket_app/components.cmake)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_bsp_atoms3")
idf_build_set_property(MINIMAL_BUILD ON)
project(m5_atoms3)
