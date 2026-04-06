cmake_minimum_required(VERSION 3.10)
project(GtsScene2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(GtsScene2 ${APP_SOURCES})

target_include_directories(GtsScene2 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)
target_link_libraries(GtsScene2 PRIVATE GravitasEngine)

set_target_properties(GtsScene2 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${GTS_ENGINE_ROOT}"
)
if (WIN32)
    set_target_properties(GtsScene2 PROPERTIES OUTPUT_NAME "GtsScene2.exe")
endif()
