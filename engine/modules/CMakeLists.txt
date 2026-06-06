# rendering must be first — it defines gravitas_rendering (and transitively
# gravitas_vulkan_backend + gravitas_vulkan_setup) before other modules reference graphics types.
add_subdirectory(rendering)
add_subdirectory(physics)
add_subdirectory(debugdraw)
add_subdirectory(tools)

# ── gravitas_modules — INTERFACE aggregator for ECS feature modules ────────────
# All sub-modules are header-only; no .cpp files exist under camera/, transform/,
# hierarchy/, animation/.
add_library(gravitas_modules INTERFACE)

target_compile_features(gravitas_modules INTERFACE cxx_std_20)

target_include_directories(gravitas_modules INTERFACE
    # Animation
    "${CMAKE_CURRENT_SOURCE_DIR}/animation/components"

    # Camera
    "${CMAKE_CURRENT_SOURCE_DIR}/camera/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/camera/debug"
    "${CMAKE_CURRENT_SOURCE_DIR}/camera/systems"

    # Hierarchy
    "${CMAKE_CURRENT_SOURCE_DIR}/hierarchy/components"

    # Transform
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/components"

    # Physics
    "${CMAKE_CURRENT_SOURCE_DIR}/physics/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics/core/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics/debug"

    # Engine tooling
    "${CMAKE_CURRENT_SOURCE_DIR}/debugdraw/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/debugdraw/primitives"
    "${CMAKE_CURRENT_SOURCE_DIR}/debugdraw/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/inspectors"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/assets"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/selection"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/debugdraw"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/gizmos"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/cameras"
)

target_link_libraries(gravitas_modules INTERFACE
    gravitas_core
    gravitas_rendering
    gravitas_physics
    gravitas_debugdraw
    gravitas_tools
)
