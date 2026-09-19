include("${GTS_ENGINE_ROOT}/cmake/GravitasGlfw.cmake")

# Add selected backend targets.
if(GTS_ENABLE_VULKAN_BACKEND)
    add_subdirectory(backend/vulkan)
endif()

# ── gravitas_rendering ─────────────────────────────────────────────────────────
# ECS rendering layer + windowing (GLFW window management).
# core/ and ecssetup/ are entirely owned by this target — GLOB_RECURSE is safe.
# CPU asset import/loading/cooking implementations are owned by assets targets.
file(GLOB_RECURSE RENDERING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp"
)
list(APPEND RENDERING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/stb_image_write_impl.cpp"
)
add_library(gravitas_rendering STATIC ${RENDERING_SOURCES})

target_compile_features(gravitas_rendering PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_rendering PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_rendering PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_rendering PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks"

    # Font loader utilities (BitmapFont, BitmapFontLoader, GlyphLayoutEngine)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/fontloader"

    # Graphics config / init headers
    "${CMAKE_CURRENT_SOURCE_DIR}/core/commands"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/lighting"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/core/windowing/windowmanager"

    # Scene-level renderer feature installer
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"

    # Engine runtime integration
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"

    # ECS rendering components and systems
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/debug"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/camera/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/extraction"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/extraction/frustumcull"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/extraction/frustumcull/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/geometry/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/geometry/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/lighting/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/lighting/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/assets"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/extraction"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecssetup/particles/systems"

    # Engine UI system + renderer-side UI extraction
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ui"

)

target_link_libraries(gravitas_rendering PUBLIC
    glfw
    gravitas_core
    gravitas_rendering_window_contracts
    gravitas_ui
    gravitas_rendering_contracts
    gravitas_rendering_command_contracts
    gravitas_rendering_controller_contracts
    gravitas_profiling
    gravitas_transform
    PUBLIC gravitas_model_domain gravitas_cooked_assets gravitas_image_decode
)

if(TARGET gravitas_vulkan_backend)
    target_link_libraries(gravitas_vulkan_backend PRIVATE
        gravitas_rendering
    )
endif()

target_link_libraries(gravitas_rendering PUBLIC gravitas_model_frontend)


target_link_libraries(gravitas_rendering PUBLIC gravitas_rendering_execution_contracts)

target_include_directories(gravitas_rendering PRIVATE "${GTS_ENGINE_ROOT}/external/stb")
