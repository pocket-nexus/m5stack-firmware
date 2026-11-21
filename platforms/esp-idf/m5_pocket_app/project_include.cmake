include_guard(GLOBAL)

set(M5_POCKET_APP_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}")

# m5_strict_warnings(<target>)
#
# The warning rule of AGENTS.md for a component library. IDF already passes
# -Wall, -Wextra and -Werror, then relaxes them with -Wno-unused-parameter,
# -Wno-sign-compare, -Wno-enum-conversion and -Wno-error=<group>; CMake drops
# a repeated -Wall/-Wextra/-Werror, so repeating them changes nothing. These
# options follow IDF's and turn those warnings back on, as errors.
function(m5_strict_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Werror
        -Wunused-parameter -Wsign-compare
        $<$<COMPILE_LANGUAGE:C>:-Wenum-conversion>
        -Werror=unused-function -Werror=unused-variable
        -Werror=unused-but-set-variable -Werror=deprecated-declarations
        -Werror=extra)
endfunction()

# m5_pocket_app(TARGET <target> NAME <name> HOST_CONFIG <file>
#               [PROJECT_ROOT <dir>] [AOT_CRATE <dir>] [DEPENDS <file>...])
#
# Embed the `app` section of HOST_CONFIG into TARGET (the program's main
# component library) in the mode CONFIG_M5_POCKET_APP_MODE selects, and
# generate m5_pocket_app_embedded() for it.
#
# JS mode: pocketjs_compile_app compiles the bundle and exports the host
# contract (pocket_profile.h).
# AOT mode: pocketjs_profile_header exports the host contract (pocket_profile.h
# and pocket_profile.rs), `pocket compile --aot` generates the MicroTS Rust
# sources (rerun from its depfile), and Cargo builds the AOT_CRATE static
# library (default: <project>/aot) on every build; Cargo itself decides what is
# stale. The library is copied, prepared for the IDF link (ESP32-P4: soft-float
# compiler-rt members removed) and linked by pocket_runtime in place of its
# default archive. Cargo runs through `rustup run <toolchain>`: esp32s3 uses
# M5_POCKET_AOT_TOOLCHAIN (default `esp`, with -Zbuild-std=core,alloc), esp32p4
# the channel of third_party/pocketjs/rust-toolchain.toml.
function(m5_pocket_app)
    cmake_parse_arguments(APP "" "TARGET;NAME;HOST_CONFIG;PROJECT_ROOT;AOT_CRATE" "DEPENDS" ${ARGN})
    if(APP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown m5_pocket_app arguments: ${APP_UNPARSED_ARGUMENTS}")
    endif()
    foreach(required TARGET NAME HOST_CONFIG)
        if(NOT APP_${required})
            message(FATAL_ERROR "m5_pocket_app requires ${required}")
        endif()
    endforeach()
    if(NOT APP_NAME MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR "m5_pocket_app NAME must match [a-z][a-z0-9_]*")
    endif()
    get_filename_component(config "${APP_HOST_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(APP_PROJECT_ROOT)
        get_filename_component(project_root "${APP_PROJECT_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        get_filename_component(project_root "${config}" DIRECTORY)
    endif()
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/m5_pocket_app/${APP_NAME}")
    set(profile_dir "${CMAKE_CURRENT_BINARY_DIR}/pocketjs/${APP_NAME}/profile")

    if(CONFIG_M5_POCKET_APP_MODE_AOT)
        if(NOT M5_POCKET_AOT_ARCHIVE)
            message(FATAL_ERROR "m5_pocket_app: AOT mode requires the project CMakeLists.txt to include m5_pocket_app/components.cmake before project()")
        endif()
        pocketjs_profile_header(TARGET "${APP_TARGET}" NAME "${APP_NAME}"
            HOST_CONFIG "${config}" PROFILE_EXPORT_DIR "${profile_dir}" DEPENDS ${APP_DEPENDS})
        if(APP_AOT_CRATE)
            get_filename_component(crate "${APP_AOT_CRATE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        else()
            set(crate "${CMAKE_SOURCE_DIR}/aot")
        endif()
        _m5_pocket_app_aot("${APP_TARGET}" "${APP_NAME}" "${config}" "${project_root}"
            "${crate}" "${profile_dir}" "${generated}" "${APP_DEPENDS}")
        set(mode M5_POCKET_APP_MODE_AOT)
        set(bundle_include "")
        set(bundle_fields "")
        set(viewport_fields
            "      .physical_width = pocket_profile.facts->display->physical[0],\n"
            "      .physical_height = pocket_profile.facts->display->physical[1],\n"
            "      .logical_width = pocket_profile.facts->display->logical[0],\n"
            "      .logical_height = pocket_profile.facts->display->logical[1],\n")
    else()
        set(compile_args TARGET "${APP_TARGET}" NAME "${APP_NAME}" HOST_CONFIG "${config}"
            PROJECT_ROOT "${project_root}" PROFILE_EXPORT_DIR "${profile_dir}")
        if(APP_DEPENDS)
            list(APPEND compile_args DEPENDS ${APP_DEPENDS})
        endif()
        pocketjs_compile_app(${compile_args})
        set(mode M5_POCKET_APP_MODE_JS)
        set(bundle_include "#include \"pocketjs_bundle_${APP_NAME}.h\"\n")
        set(bundle_fields
            "      .bundle = pocketjs_bundle_${APP_NAME}.data,\n"
            "      .bundle_size = pocketjs_bundle_${APP_NAME}.size,\n")
        set(viewport_fields
            "      .physical_width = pocketjs_bundle_${APP_NAME}_contract.physical_width,\n"
            "      .physical_height = pocketjs_bundle_${APP_NAME}_contract.physical_height,\n"
            "      .logical_width = pocketjs_bundle_${APP_NAME}_contract.logical_width,\n"
            "      .logical_height = pocketjs_bundle_${APP_NAME}_contract.logical_height,\n")
    endif()
    string(JOIN "" bundle_fields ${bundle_fields})
    string(JOIN "" viewport_fields ${viewport_fields})
    file(GENERATE OUTPUT "${generated}/m5_pocket_app_image.c" CONTENT
"/* Generated by m5_pocket_app(); do not edit. */
#include \"m5_pocket/app.h\"
#include \"pocket_profile.h\"
${bundle_include}
const m5_pocket_app_image *m5_pocket_app_embedded(void) {
  static m5_pocket_app_image image;
  if (!image.struct_size) {
    image = (m5_pocket_app_image){
      .struct_size = sizeof(image),
      .name = \"${APP_NAME}\",
      .mode = ${mode},
${bundle_fields}      .facts = pocket_profile.facts,
      .profile_hash = pocket_profile.profile_hash,
      .host_abi = pocket_profile.host_abi,
      .tick_hz = pocket_profile.tick_hz,
${viewport_fields}    };
  }
  return &image;
}
")
    target_sources("${APP_TARGET}" PRIVATE "${generated}/m5_pocket_app_image.c")
endfunction()

function(_m5_pocket_app_aot target name config project_root crate profile_dir generated depends)
    if(NOT EXISTS "${crate}/Cargo.toml")
        message(FATAL_ERROR "m5_pocket_app: no AOT crate at ${crate} (AOT_CRATE)")
    endif()
    set(pins "${M5_POCKET_POCKETJS_DIR}/rust-toolchain.toml")
    if(IDF_TARGET STREQUAL "esp32s3")
        set(triple "xtensa-esp32s3-none-elf")
        set(default_toolchain "esp")
        set(unstable "-Zbuild-std=core,alloc")
    elseif(IDF_TARGET STREQUAL "esp32p4")
        set(triple "riscv32imafc-unknown-none-elf")
        file(STRINGS "${pins}" channel REGEX "^channel *=")
        string(REGEX REPLACE "^channel *= *\"([^\"]+)\".*$" "\\1" default_toolchain "${channel}")
        set(unstable "")
    else()
        message(FATAL_ERROR "m5_pocket_app: AOT mode supports esp32s3 and esp32p4, not ${IDF_TARGET}")
    endif()
    set(M5_POCKET_AOT_TOOLCHAIN "${default_toolchain}" CACHE STRING
        "rustup toolchain that builds the AOT library (esp32s3: Espressif's Xtensa toolchain)")
    find_program(M5_POCKET_RUSTUP rustup HINTS "$ENV{HOME}/.cargo/bin" REQUIRED)

    # `pocket compile --aot` reads the core's layout oracle, a wasm build of
    # the submodule (ignored by its .gitignore). Build it once when missing.
    set(oracle "${M5_POCKET_POCKETJS_DIR}/platforms/web/pocketjs.wasm")
    if(NOT EXISTS "${oracle}" AND NOT TARGET m5_pocket_aot_oracle)
        find_program(M5_POCKET_BUN bun HINTS "$ENV{HOME}/.bun/bin" REQUIRED)
        add_custom_command(
            OUTPUT "${oracle}"
            COMMAND "${M5_POCKET_BUN}" tools/wasm.ts
            WORKING_DIRECTORY "${M5_POCKET_POCKETJS_DIR}"
            COMMENT "PocketJS: building the MicroTS layout oracle (bun tools/wasm.ts in third_party/pocketjs)"
            USES_TERMINAL
            VERBATIM)
        add_custom_target(m5_pocket_aot_oracle DEPENDS "${oracle}")
    endif()

    set(aot_dir "${generated}/aot")
    add_custom_command(
        OUTPUT "${aot_dir}/include.rs"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${aot_dir}"
        COMMAND "${POCKETJS_CLI}" compile --config "${config}" --project-root "${project_root}"
            --plan-dir "${generated}/plans" --aot --out "${aot_dir}" --depfile "${aot_dir}.d"
        DEPFILE "${aot_dir}.d"
        DEPENDS "${config}" "${POCKETJS_CLI}" "${oracle}" ${depends}
        COMMENT "MicroTS: compiling ${name} to Rust"
        VERBATIM)

    set(cargo_dir "${CMAKE_BINARY_DIR}/m5_pocket_aot/cargo")
    set(raw "${cargo_dir}/${triple}/release/libm5_pocket_app_aot.a")
    add_custom_target(m5_pocket_aot_${name}
        COMMAND "${CMAKE_COMMAND}" -E env
            "CARGO_TARGET_DIR=${cargo_dir}"
            "POCKETJS_AOT_DIR=${aot_dir}"
            "POCKETJS_PROFILE_RS=${profile_dir}/pocket_profile.rs"
            "RUSTFLAGS=" "CARGO_ENCODED_RUSTFLAGS=" "RUSTC_WRAPPER=" "RUSTC_WORKSPACE_WRAPPER="
            "${M5_POCKET_RUSTUP}" run "${M5_POCKET_AOT_TOOLCHAIN}"
            cargo build ${unstable} --release --locked --target "${triple}"
            --manifest-path "${crate}/Cargo.toml"
        COMMAND "${CMAKE_COMMAND}" "-DRAW=${raw}" "-DOUT=${M5_POCKET_AOT_ARCHIVE}"
            "-DARCHIVER=${CMAKE_AR}" "-DIDF_TARGET=${IDF_TARGET}"
            "-DPREPARE=${M5_POCKET_POCKETJS_DIR}/platforms/esp-idf/pocket_runtime/prepare_archive.cmake"
            -P "${M5_POCKET_APP_COMPONENT_DIR}/aot_archive.cmake"
        BYPRODUCTS "${M5_POCKET_AOT_ARCHIVE}"
        DEPENDS "${aot_dir}/include.rs" "${profile_dir}/pocket_profile.h"
        WORKING_DIRECTORY "${crate}"
        COMMENT "Cargo: building the ${name} AOT library for ${triple}"
        USES_TERMINAL
        VERBATIM)
    add_dependencies("${target}" m5_pocket_aot_${name})
    idf_component_get_property(runtime_lib pocket_runtime COMPONENT_LIB)
    add_dependencies("${runtime_lib}" m5_pocket_aot_${name})
endfunction()
