cmake_minimum_required(VERSION 3.5.0)
project(Gravitas VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

add_executable(Gravitas main.cpp)

target_link_libraries(Gravitas glfw)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Link against Vulkan
target_link_libraries(Gravitas Vulkan::Vulkan)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
