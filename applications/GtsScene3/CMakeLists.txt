cmake_minimum_required(VERSION 3.10)
project(GtsScene3 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(GtsScene3 ${APP_SOURCES})

target_include_directories(GtsScene3 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(GtsScene3 PRIVATE GravitasEngine)

set_target_properties(GtsScene3 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
if (WIN32)
    set_target_properties(GtsScene3 PROPERTIES OUTPUT_NAME "GtsScene3.exe")
endif()
