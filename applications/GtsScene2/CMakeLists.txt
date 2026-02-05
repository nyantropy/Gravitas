cmake_minimum_required(VERSION 3.10)

project(GtsScene2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    *.cpp
    *.c
)

add_executable(GtsScene2 ${APP_SOURCES})
target_link_libraries(GtsScene2 PRIVATE GravitasEngine)

set_target_properties(GtsScene2 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    set_target_properties(GtsScene2 PROPERTIES OUTPUT_NAME "GtsScene2.exe")
endif()