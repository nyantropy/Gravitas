cmake_minimum_required(VERSION 3.10)

project(GravitasTetris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    *.cpp
    *.c
    *.hpp
)

add_executable(GravitasTetris ${APP_SOURCES})
target_link_libraries(GravitasTetris PRIVATE GravitasEngine)

set_target_properties(GravitasTetris PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    set_target_properties(GravitasTetris PROPERTIES OUTPUT_NAME "GravitasTetris.exe")
endif()