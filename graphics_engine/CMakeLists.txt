# Parent CMakeLists.txt

cmake_minimum_required(VERSION 3.5.0)

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

# Include directory for stb_image.h, stb_image_write.h and tiny_obj_loader.h
set(STB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/stb CACHE INTERNAL "")

# Include directory for glm
set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/glm/glm CACHE INTERNAL "")

# Source files for the library are all .c .h .cpp files
file(GLOB GravitasSRC ./*.cpp ./*.c ./*.h)
add_library(Gravitas STATIC ${GravitasSRC})
add_dependencies(Gravitas GLFW)

# Set all the include directories for this target
target_include_directories(Gravitas PUBLIC
    ${GLM_INCLUDE_DIR} 
    ${GLFW_INCLUDE_DIRS}
    ${GLFW_LIB_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${STB_INCLUDE_DIR}
)

# Link all the required libraries
target_link_libraries(Gravitas 
    Vulkan::Vulkan
    instance
    PkgConfig::LIBAV
    $<$<PLATFORM_ID:Windows>:${GLFW_LIB_DIR}/glfw3.lib> # Windows
    $<$<NOT:$<PLATFORM_ID:Windows>>:${GLFW_LIB_DIR}/libglfw3.a> # Linux
)

add_subdirectory(instance)

# Output dirs
set_target_properties(Gravitas PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

# CPack settings
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
