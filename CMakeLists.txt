cmake_minimum_required(VERSION 3.16)
project(FontyDE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FONTYDE_WOFF2 "Enable WOFF2 support via brotli" OFF)
option(FONTYDE_BUILD_TESTS "Build unit tests" OFF)

find_package(ZLIB REQUIRED)

if(FONTYDE_WOFF2)
    find_package(unofficial-brotli CONFIG REQUIRED)
endif()

file(GLOB_RECURSE FONTYDE_SOURCES
    "src/*.cpp"
    "src/tables/*.cpp"
    "src/formats/*.cpp"
    "src/emitter/*.cpp"
)

add_executable(fontyde ${FONTYDE_SOURCES})

target_include_directories(fontyde PRIVATE include)
target_link_libraries(fontyde PRIVATE ZLIB::ZLIB)

if(FONTYDE_WOFF2)
    target_link_libraries(fontyde PRIVATE unofficial::brotli::brotlidec)
    target_compile_definitions(fontyde PRIVATE FONTYDE_ENABLE_WOFF2)
endif()

if(MSVC)
    target_compile_options(fontyde PRIVATE /W4 /WX)
else()
    target_compile_options(fontyde PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS fontyde RUNTIME DESTINATION bin)
install(FILES README.md DESTINATION share/fontyde)
install(DIRECTORY docs/ DESTINATION share/fontyde/docs)
