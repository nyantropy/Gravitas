find_package(Vulkan REQUIRED)

# Add backend — defines gravitas_vulkan_setup and gravitas_vulkan_backend.
add_subdirectory(backend/vulkan)

# ── gravitas_rendering ─────────────────────────────────────────────────────────
# ECS rendering layer + windowing (GLFW window management).
# core/ and ecssetup/ are entirely owned by this target — GLOB_RECURSE is safe.
# Currently only GLFWOutputWindow.cpp exists; all other files are header-only.
file(GLOB_RECURSE RENDERING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/*.cpp"
)
add_library(gravitas_rendering STATIC ${RENDERING_SOURCES})

target_compile_features(gravitas_rendering PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_rendering PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_rendering PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_rendering PUBLIC
    # Rendering module root (VulkanGraphics.hpp is in core/vulkangraphics/)
    "${CMAKE_CURRENT_SOURCE_DIR}"

    # Font loader utilities (BitmapFont, BitmapFontLoader, GlyphLayoutEngine)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/fontloader"

    # Graphics config / init headers
    "${CMAKE_CURRENT_SOURCE_DIR}/core/init"

    # Abstract interfaces (IGtsGraphicsModule, IResourceProvider)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/interface"

    # Concrete Vulkan graphics wrapper
    "${CMAKE_CURRENT_SOURCE_DIR}/core/vulkangraphics"

    # Windowing
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/glfwWindow"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/output"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/windowmanager"

    # ECS rendering components and systems
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/systems"
)

target_link_libraries(gravitas_rendering PUBLIC
    gravitas_vulkan_backend
    gravitas_core
)
