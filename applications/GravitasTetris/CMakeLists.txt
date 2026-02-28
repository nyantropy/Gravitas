cmake_minimum_required(VERSION 3.10)
project(GravitasTetris LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(GravitasTetris ${APP_SOURCES})

target_include_directories(GravitasTetris PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)
target_link_libraries(GravitasTetris PRIVATE GravitasEngine)

set_target_properties(GravitasTetris PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
if (WIN32)
    set_target_properties(GravitasTetris PROPERTIES OUTPUT_NAME "GravitasTetris.exe")
endif()
