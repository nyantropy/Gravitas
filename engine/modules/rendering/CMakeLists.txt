cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(Vulkan REQUIRED)

# ── Low-level graphics backend (aggregation target) ───────────────────────────
# Defined first so subdirectory CMakeLists can reference it as a known target.

file(GLOB graphicsSRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
add_library(graphics STATIC ${graphicsSRC})

add_subdirectory(external)
add_subdirectory(backend/vulkan/vksetup)
add_subdirectory(backend/vulkan/rendering)
add_subdirectory(backend/vulkan/util)
add_subdirectory(backend/vulkan/texture)
add_subdirectory(windowing)
add_subdirectory(resources)

target_include_directories(graphics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(graphics
    Vulkan::Vulkan
    vksetup
    renderresource
    event
    core
    rendering
    graphicsutil
    windowing
    external
)

set_target_properties(graphics PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

# ── ECS-facing rendering layer (components, systems, extractors) ──────────────

file(GLOB RENDERING_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)
file(GLOB RENDERING_SYSTEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp"
)
file(GLOB RENDERING_EXTRACTORS
    "${CMAKE_CURRENT_SOURCE_DIR}/extractors/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/extractors/*.hpp"
)

add_library(ecsrendering ${RENDERING_COMPONENTS} ${RENDERING_SYSTEMS} ${RENDERING_EXTRACTORS})

target_include_directories(ecsrendering PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/extractors"
)
target_link_libraries(ecsrendering PUBLIC ecsworld ecstransform ecscamera graphics)
