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

# Source files for the library are all .c .h .cpp files
file(GLOB GravitasSRC ./*.cpp ./*.c ./*.h)
add_library(Gravitas STATIC ${GravitasSRC})

add_subdirectory(external)
add_subdirectory(vksetup)
add_subdirectory(event)
add_subdirectory(options)
add_subdirectory(rendering)
add_subdirectory(graphics_util)
add_subdirectory(windowing)
add_subdirectory(gsystems)
add_subdirectory(gcomponents)
add_subdirectory(renderresource)



# Set all the include directories for this target
target_include_directories(Gravitas PUBLIC
    ${CMAKE_SOURCE_DIR}/graphics_module
    ${Vulkan_INCLUDE_DIRS}
)

# Link all the required libraries
target_link_libraries(Gravitas 
    Vulkan::Vulkan
    vksetup
    renderresource
    event
    rendering
    graphicsutil
    windowing
    external
    options
    gsystems
    gcomponents
    PkgConfig::LIBAV
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
