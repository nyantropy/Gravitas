# ── GLFW (FetchContent — single declaration for the whole build) ───────────────
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        latest
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

find_package(Vulkan REQUIRED)

# ── gravitas_vulkan_setup ──────────────────────────────────────────────────────
# Instance, surface, physical/logical device, swapchain.
# The entire vksetup/ tree belongs to this target — GLOB_RECURSE is safe.
# VulkanContext.cpp exists but is empty (context logic lives in the header);
# it is harmlessly picked up and compiles as an empty translation unit.
file(GLOB_RECURSE VULKAN_SETUP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
add_library(gravitas_vulkan_setup STATIC ${VULKAN_SETUP_SOURCES})

target_compile_features(gravitas_vulkan_setup PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_vulkan_setup PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_vulkan_setup PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Platform definitions
if(WIN32)
    target_compile_definitions(gravitas_vulkan_setup PUBLIC
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND QUIET wayland-client)
    endif()
    if(WAYLAND_FOUND)
        target_compile_definitions(gravitas_vulkan_setup PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)
    else()
        target_compile_definitions(gravitas_vulkan_setup PUBLIC VK_USE_PLATFORM_XLIB_KHR)
    endif()
endif()

target_include_directories(gravitas_vulkan_setup PUBLIC
    # vksetup sub-directories
    "${CMAKE_CURRENT_SOURCE_DIR}/context"
    "${CMAKE_CURRENT_SOURCE_DIR}/instance"
    "${CMAKE_CURRENT_SOURCE_DIR}/surface"
    "${CMAKE_CURRENT_SOURCE_DIR}/physicaldevice"
    "${CMAKE_CURRENT_SOURCE_DIR}/logicaldevice"
    "${CMAKE_CURRENT_SOURCE_DIR}/swapchain"

    # VulkanSwapChainConfig.h and VulkanContextConfig.h include OutputWindow.hpp
    "${GTS_ENGINE_ROOT}/engine/modules/rendering/core/windowing/output"
    # OutputWindow.hpp includes GtsEvent.hpp and GtsKeyEvent.h
    "${GTS_ENGINE_ROOT}/engine/core/event"
    "${GTS_ENGINE_ROOT}/engine/core/input/raw"
)

target_link_libraries(gravitas_vulkan_setup PUBLIC
    Vulkan::Vulkan
    glfw
)
