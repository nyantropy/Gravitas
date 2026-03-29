cmake_minimum_required(VERSION 3.10)
project(DungeonCrawler LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(DungeonCrawler ${APP_SOURCES})

target_include_directories(DungeonCrawler PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/struct"
    "${CMAKE_CURRENT_SOURCE_DIR}/generator"
)
target_link_libraries(DungeonCrawler PRIVATE GravitasEngine)

set_target_properties(DungeonCrawler PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
if (WIN32)
    set_target_properties(DungeonCrawler PROPERTIES OUTPUT_NAME "DungeonCrawler.exe")
endif()
