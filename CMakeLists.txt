cmake_minimum_required(VERSION 3.16)
project(FontyDE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FONTYDE_WOFF2 "Enable WOFF2 support via brotli"    OFF)
option(FONTYDE_WEB   "Build Emscripten WebAssembly target" OFF)

# No separate include/ directory exists.
# All headers live inside src/fontyde/ (sibling to the .cpp files).
# Every #include uses a relative path so no -I flag is needed at all:
#   src/main.cpp            uses  "fontyde/X"      -> finds src/fontyde/X
#   src/tables/foo.cpp      uses  "../fontyde/X"   -> finds src/fontyde/X
#   src/fontyde/font.h      uses  "tables/X"       -> finds src/fontyde/tables/X
#   src/fontyde/tables/X.h  uses  "../reader.h"    -> finds src/fontyde/reader.h
# This works on every compiler, OS, and wrapper (emcmake, NDK, MSVC).

if(ANDROID)
    find_library(ZLIB_LIBRARY z)
    if(NOT ZLIB_LIBRARY)
        message(FATAL_ERROR "zlib not found in Android NDK sysroot")
    endif()
elseif(FONTYDE_WEB)
    # Emscripten provides its own zlib port via -sUSE_ZLIB=1
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
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)

add_executable(fontyde ${FONTYDE_SOURCES})

# src/ is the include root — resolves "fontyde/X" to src/fontyde/X
target_include_directories(fontyde PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
)

if(ANDROID)
    target_link_libraries(fontyde PRIVATE ${ZLIB_LIBRARY})
elseif(FONTYDE_WEB)
    set_target_properties(fontyde PROPERTIES OUTPUT_NAME "fontyde" SUFFIX ".js")
    target_compile_options(fontyde PRIVATE -O2 -sUSE_ZLIB=1)
    target_link_options(fontyde PRIVATE
        -sUSE_ZLIB=1
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','IDBFS']
        -sEXPORTED_FUNCTIONS=['_main']
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web,worker
        -sINITIAL_MEMORY=33554432
        --shell-file "${CMAKE_CURRENT_LIST_DIR}/web/shell.html"
        --pre-js     "${CMAKE_CURRENT_LIST_DIR}/web/pre.js"
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
