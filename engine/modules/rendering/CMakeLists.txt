include("${GTS_ENGINE_ROOT}/cmake/GravitasGlfw.cmake")

# Add selected backend targets.
if(GTS_ENABLE_VULKAN_BACKEND)
    add_subdirectory(backend/vulkan)
endif()

# ── gravitas_rendering ─────────────────────────────────────────────────────────
# ECS rendering layer + windowing (GLFW window management).
# core/ and ecssetup/ are entirely owned by this target — GLOB_RECURSE is safe.
# Currently only GLFWOutputWindow.cpp exists; all other files are header-only.
file(GLOB_RECURSE RENDERING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp"
)
add_library(gravitas_rendering STATIC ${RENDERING_SOURCES})

target_compile_features(gravitas_rendering PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_rendering PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_rendering PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_rendering PUBLIC
    # Rendering module root
    "${CMAKE_CURRENT_SOURCE_DIR}"

    # Font loader utilities (BitmapFont, BitmapFontLoader, GlyphLayoutEngine)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/fontloader"

    # Graphics config / init headers
    "${CMAKE_CURRENT_SOURCE_DIR}/core/commands"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/geometry"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/init"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/material"

    # Abstract interfaces (IGtsGraphicsModule, IResourceProvider)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/interface"

    # Render viewport contract
    "${CMAKE_CURRENT_SOURCE_DIR}/core/viewport"

    # Render pass visibility controls
    "${CMAKE_CURRENT_SOURCE_DIR}/core/visibility"

    # Editor/tool preview render packets
    "${CMAKE_CURRENT_SOURCE_DIR}/core/preview"

    # Windowing
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/glfwWindow"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/output"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/windowmanager"

    # Scene-level renderer feature installer
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"

    # Engine runtime integration
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"

    # ECS rendering components and systems
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/debug"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/extraction"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/extraction/frustumcull"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/extraction/frustumcull/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/geometry/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/geometry/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/assets"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/extraction"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/systems"

    # Cross-module component contracts used by rendering systems
    "${CMAKE_CURRENT_SOURCE_DIR}/../transform/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/../transform/hierarchy"
    "${CMAKE_CURRENT_SOURCE_DIR}/../transform/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/../transform/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/../transform/systems"

    # Engine UI system + renderer-side UI extraction
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ui"
)

target_link_libraries(gravitas_rendering PUBLIC
    glfw
    gravitas_core
)

if(TARGET gravitas_vulkan_backend)
    target_link_libraries(gravitas_vulkan_backend PUBLIC
        gravitas_rendering
    )
endif()
