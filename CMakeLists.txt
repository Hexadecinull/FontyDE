cmake_minimum_required(VERSION 3.16)
project(FontyDE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FONTYDE_WOFF2 "Enable WOFF2 support via brotli"    OFF)
option(FONTYDE_WEB   "Build Emscripten WebAssembly target" OFF)

# CMAKE_CURRENT_LIST_DIR is always the absolute path to the directory
# containing this file, regardless of how CMake was invoked or whether
# a wrapper (emcmake, toolchain file, etc.) changed CMAKE_CURRENT_SOURCE_DIR.
set(FONTYDE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FONTYDE_INCLUDE_DIR "${FONTYDE_ROOT}/include")

message(STATUS "FontyDE root       : ${FONTYDE_ROOT}")
message(STATUS "FontyDE include dir: ${FONTYDE_INCLUDE_DIR}")

if(NOT IS_DIRECTORY "${FONTYDE_INCLUDE_DIR}")
    message(FATAL_ERROR
        "Include directory not found: ${FONTYDE_INCLUDE_DIR}\n"
        "Make sure the full FontyDE source tree is present (not just the CMakeLists.txt).")
endif()

if(ANDROID)
    find_library(ZLIB_LIBRARY z)
    if(NOT ZLIB_LIBRARY)
        message(FATAL_ERROR "zlib not found in Android NDK sysroot")
    endif()
elseif(FONTYDE_WEB)
    # Emscripten bundles its own zlib — no find_package needed
else()
    find_package(ZLIB REQUIRED)
endif()

if(FONTYDE_WOFF2 AND NOT FONTYDE_WEB)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(BROTLI REQUIRED libbrotlidec)
    else()
        find_library(BROTLI_LIBRARIES NAMES brotlidec libbrotlidec REQUIRED)
        find_path(BROTLI_INCLUDE_DIRS brotli/decode.h REQUIRED)
    endif()
endif()

file(GLOB_RECURSE FONTYDE_SOURCES
    "${FONTYDE_ROOT}/src/*.cpp"
    "${FONTYDE_ROOT}/src/tables/*.cpp"
    "${FONTYDE_ROOT}/src/formats/*.cpp"
    "${FONTYDE_ROOT}/src/emitter/*.cpp"
)

if(NOT FONTYDE_SOURCES)
    message(FATAL_ERROR "No source files found under ${FONTYDE_ROOT}/src/")
endif()

message(STATUS "FontyDE sources    : ${FONTYDE_SOURCES}")

add_executable(fontyde ${FONTYDE_SOURCES})

# Set the include path globally (covers all targets and all compilers)
# and also per-target. Both use the absolute FONTYDE_INCLUDE_DIR derived
# from CMAKE_CURRENT_LIST_DIR so wrappers cannot affect the resolution.
include_directories("${FONTYDE_INCLUDE_DIR}")
target_include_directories(fontyde PRIVATE "${FONTYDE_INCLUDE_DIR}")

if(ANDROID)
    target_link_libraries(fontyde PRIVATE ${ZLIB_LIBRARY})
elseif(FONTYDE_WEB)
    set_target_properties(fontyde PROPERTIES
        OUTPUT_NAME "fontyde"
        SUFFIX      ".js"
    )
    target_compile_options(fontyde PRIVATE -O2 -sUSE_ZLIB=1)
    target_link_options(fontyde PRIVATE
        -sUSE_ZLIB=1
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','IDBFS']
        -sEXPORTED_FUNCTIONS=['_main','_fontyde_decompile_buffer']
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web,worker
        -sINITIAL_MEMORY=33554432
        --shell-file "${FONTYDE_ROOT}/web/shell.html"
        --pre-js     "${FONTYDE_ROOT}/web/pre.js"
    )
else()
    target_link_libraries(fontyde PRIVATE ZLIB::ZLIB)
endif()

if(FONTYDE_WOFF2 AND NOT FONTYDE_WEB)
    if(DEFINED BROTLI_LINK_LIBRARIES)
        target_link_libraries(fontyde PRIVATE ${BROTLI_LINK_LIBRARIES})
    else()
        target_link_libraries(fontyde PRIVATE ${BROTLI_LIBRARIES})
    endif()
    target_include_directories(fontyde PRIVATE ${BROTLI_INCLUDE_DIRS})
    target_compile_definitions(fontyde PRIVATE FONTYDE_ENABLE_WOFF2)
endif()

if(MSVC)
    target_compile_options(fontyde PRIVATE /W3)
    target_compile_definitions(fontyde PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        NOMINMAX
    )
else()
    target_compile_options(fontyde PRIVATE -Wall -Wextra)
endif()

if(NOT ANDROID AND NOT FONTYDE_WEB)
    install(TARGETS fontyde RUNTIME DESTINATION bin)
    install(FILES README.md DESTINATION share/fontyde)
    install(DIRECTORY docs/ DESTINATION share/fontyde/docs)
endif()
