cmake_minimum_required(VERSION 3.5.0)

project(GravitasApp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

#we need vulkan here as well
find_package(Vulkan REQUIRED)

#main target is just our small cpp file
add_executable(main main.cpp)
add_dependencies(main Gravitas)

#include glm as a header only library
target_include_directories(main PUBLIC ${CMAKE_SOURCE_DIR}/external/glm)
#include the graphics engine
target_include_directories(main PUBLIC ${CMAKE_SOURCE_DIR}/graphics_engine)
#include vulkan
target_include_directories(main PUBLIC ${Vulkan_INCLUDE_DIRS})

#link libraries
target_link_libraries(main Vulkan::Vulkan Gravitas)