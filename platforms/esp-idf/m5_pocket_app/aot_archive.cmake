# Copy the AOT library Cargo built to the path pocket_runtime links, and
# prepare it for the IDF link, only when Cargo produced a new archive: an
# unchanged library leaves OUT untouched, so the firmware does not relink.
#   -DRAW=<cargo output> -DOUT=<linked archive> -DARCHIVER=<ar>
#   -DIDF_TARGET=<target> -DPREPARE=<pocket_runtime/prepare_archive.cmake>
foreach(variable RAW OUT ARCHIVER IDF_TARGET PREPARE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "aot_archive.cmake requires ${variable}")
    endif()
endforeach()
if(NOT EXISTS "${RAW}")
    message(FATAL_ERROR "Cargo did not produce ${RAW}; the AOT crate must declare [lib] name = \"m5_pocket_app_aot\" and crate-type = [\"staticlib\"]")
endif()
if(EXISTS "${OUT}" AND "${OUT}" IS_NEWER_THAN "${RAW}")
    file(SIZE "${OUT}" size)
    if(size GREATER 8)
        return()
    endif()
endif()
set(staging "${OUT}.new")
file(COPY_FILE "${RAW}" "${staging}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DPOCKETJS_ARCHIVE=${staging}" "-DPOCKETJS_ARCHIVER=${ARCHIVER}"
        "-DPOCKETJS_TARGET=${IDF_TARGET}" -P "${PREPARE}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "preparing the AOT library failed")
endif()
file(RENAME "${staging}" "${OUT}")
file(TOUCH "${OUT}")
