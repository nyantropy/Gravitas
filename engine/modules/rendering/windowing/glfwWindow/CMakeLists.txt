cmake_minimum_required(VERSION 3.14)
project(glfwWindow)

file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(glfwWindow STATIC ${CURRENT_DIR_SRC})

target_include_directories(glfwWindow PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# fetch glfw to include with this part of the code, since we managed to isolate it all in this library
include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

target_link_libraries(glfwWindow PUBLIC output glfw)