cmake_minimum_required(VERSION 3.5.0)

project(GravitasApp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

#we need vulkan here as well
find_package(Vulkan REQUIRED)

#we need to find the glm package as well
find_package(glm CONFIG REQUIRED)

#include directories
include_directories(${CMAKE_SOURCE_DIR}/graphics_engine)
include_directories(${Vulkan_INCLUDE_DIRS})

#main target is just our small cpp file
add_executable(main main.cpp)
add_dependencies(main Gravitas)

#include glm as a header only library
target_include_directories(main PUBLIC ${GLM_INCLUDE_DIRS})

#link libraries
target_link_libraries(main Vulkan::Vulkan glm Gravitas)