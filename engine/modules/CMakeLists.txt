# rendering must be first — it defines gravitas_rendering and any selected
# backend before other modules reference graphics types.
if(GTS_ENABLE_RENDERING)
    add_subdirectory(rendering)
endif()
if(GTS_ENABLE_PHYSICS)
    add_subdirectory(physics)
endif()
if(GTS_ENABLE_DEBUGDRAW)
    add_subdirectory(diagnostics/debugdraw)
endif()
if(GTS_ENABLE_TOOLS)
    add_subdirectory(tools)
endif()

# ── gravitas_modules — INTERFACE aggregator for ECS feature modules ────────────
# Lightweight ECS modules are header-only; compiled feature modules are linked below.
add_library(gravitas_modules INTERFACE)

target_compile_features(gravitas_modules INTERFACE cxx_std_20)

target_include_directories(gravitas_modules INTERFACE
    # Animation
    "${CMAKE_CURRENT_SOURCE_DIR}/animation/components"

    # Tweening
    "${CMAKE_CURRENT_SOURCE_DIR}/tween"

    # Narrative runtimes
    "${CMAKE_CURRENT_SOURCE_DIR}/narrative/dialogue/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/narrative/dialogue/events"
    "${CMAKE_CURRENT_SOURCE_DIR}/narrative/dialogue/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/narrative/dialogue/systems"

    # Hierarchy
    "${CMAKE_CURRENT_SOURCE_DIR}/hierarchy/components"

    # Transform
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/components"

    # Visual novel presentation/runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/visualnovel/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/visualnovel/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/visualnovel/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/visualnovel/ui"
)

target_link_libraries(gravitas_modules INTERFACE
    gravitas_core
)

foreach(GTS_OPTIONAL_MODULE_TARGET
        gravitas_rendering
        gravitas_physics
        gravitas_debugdraw
        gravitas_tools)
    if(TARGET ${GTS_OPTIONAL_MODULE_TARGET})
        target_link_libraries(gravitas_modules INTERFACE ${GTS_OPTIONAL_MODULE_TARGET})
    endif()
endforeach()
