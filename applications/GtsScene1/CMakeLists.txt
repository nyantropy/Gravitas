cmake_minimum_required(VERSION 3.10)

project(GtsScene1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    *.cpp
    *.c
    *.hpp
)

add_executable(GtsScene1 ${APP_SOURCES})
target_link_libraries(GtsScene1 PRIVATE GravitasEngine)

set_target_properties(GtsScene1 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    set_target_properties(GtsScene1 PROPERTIES OUTPUT_NAME "GtsScene1.exe")
endif()