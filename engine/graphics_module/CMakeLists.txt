cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Find Vulkan on the system
find_package(Vulkan REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

# GLFW will be compiled as a separate project
include(ExternalProject)
ExternalProject_Add(GLFW
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/glfw
    INSTALL_DIR ${CMAKE_BINARY_DIR}/glfw
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

# Get GLFW include directories
ExternalProject_Get_Property(GLFW SOURCE_DIR)
get_filename_component(GLFW_ROOT_DIR "${SOURCE_DIR}/../.." ABSOLUTE)
set(GLFW_INCLUDE_DIRS ${GLFW_ROOT_DIR}/include CACHE INTERNAL "")
set(GLFW_LIB_DIR ${GLFW_ROOT_DIR}/lib CACHE INTERNAL "")

# Source files for the library are all .c .h .cpp files
file(GLOB GravitasSRC ./*.cpp ./*.c ./*.h)
add_library(Gravitas STATIC ${GravitasSRC})
add_dependencies(Gravitas GLFW)

#global include directories for way less overhead, dont know if thats a good approach but it works for me
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/surface"
    "${CMAKE_CURRENT_SOURCE_DIR}/scenegraph"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering"
    "${CMAKE_CURRENT_SOURCE_DIR}/options"
    "${CMAKE_CURRENT_SOURCE_DIR}/graphics_util"
)

add_subdirectory(external)
add_subdirectory(scenegraph)
add_subdirectory(vksetup)
add_subdirectory(event)
add_subdirectory(options)
add_subdirectory(rendering)
add_subdirectory(graphics_util)
add_subdirectory(windowing)


# Set all the include directories for this target
target_include_directories(Gravitas PUBLIC
    ${CMAKE_SOURCE_DIR}/graphics_engine
    ${GLFW_INCLUDE_DIRS}
    ${GLFW_LIB_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

# Link all the required libraries
target_link_libraries(Gravitas 
    Vulkan::Vulkan
    vksetup
    event
    rendering
    graphicsutil
    windowing
    external
    scenegraph
    options
    PkgConfig::LIBAV
    $<$<PLATFORM_ID:Windows>:${GLFW_LIB_DIR}/glfw3.lib> # Windows
    $<$<NOT:$<PLATFORM_ID:Windows>>:${GLFW_LIB_DIR}/libglfw3.a> # Linux
)


# Output dirs
set_target_properties(Gravitas PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

# CPack settings
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
