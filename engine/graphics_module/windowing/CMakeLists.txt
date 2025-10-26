cmake_minimum_required(VERSION 3.14)
project(windowing)

add_library(windowing INTERFACE)

add_subdirectory(output)
add_subdirectory(surface)
add_subdirectory(windowmanager)

target_link_libraries(windowing INTERFACE output surface windowmanager)
target_include_directories(windowing INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# fetch glfw
include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)

# turn of glfw extras
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

target_link_libraries(windowing INTERFACE glfw)
