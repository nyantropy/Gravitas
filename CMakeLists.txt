cmake_minimum_required(VERSION 3.5.0)
project(Gravitas VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

file(GLOB GravitasSRC ./*.cpp ./*.c ./*.h)
add_library(Gravitas STATIC ${GravitasSRC} ${CMAKE_SOURCE_DIR}/external/threadpool/ThreadPool.cpp)

# include directories
target_include_directories(Gravitas PUBLIC ${CMAKE_SOURCE_DIR}/external/nuklear)
target_include_directories(Gravitas PUBLIC ${CMAKE_SOURCE_DIR}/external/stb)
target_include_directories(Gravitas PUBLIC ${CMAKE_SOURCE_DIR}/external/threadpool)

find_package(glfw3 REQUIRED)
target_include_directories(Gravitas PUBLIC ${glfw_INCLUDE_DIRS})
target_link_libraries(Gravitas glfw)
message(STATUS "GLFW include directories: ${glfw_INCLUDE_DIRS}")


find_package(Vulkan REQUIRED)
target_include_directories(Gravitas PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Gravitas Vulkan::Vulkan)
message(STATUS "Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")

add_executable(main main.cpp)
target_link_libraries(main Gravitas)

set_target_properties(Gravitas PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
            )


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)