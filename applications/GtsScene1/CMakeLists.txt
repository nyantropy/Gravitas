cmake_minimum_required(VERSION 3.10)

project(GtsScene1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    *.cpp
    *.c
)

add_executable(main ${APP_SOURCES})
target_link_libraries(main PRIVATE GravitasEngine)

set_target_properties(main PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    set_target_properties(main PROPERTIES OUTPUT_NAME "main.exe")
endif()