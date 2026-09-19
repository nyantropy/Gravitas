# Layered CPU/model and reusable animation/material services precede rendering.
add_subdirectory(assets)
add_subdirectory(model)
add_subdirectory(rendering/core/material)
add_subdirectory(animation/skeletal)
add_subdirectory(animation/skinning)

# rendering must precede ECS modules — it defines gravitas_rendering and any selected
# backend before other modules reference graphics types.
if(GTS_ENABLE_RENDERING)
    add_subdirectory(rendering)
endif()
add_subdirectory(narrative)
if(GTS_ENABLE_PHYSICS)
    add_subdirectory(physics)
endif()
if(GTS_ENABLE_DEBUGDRAW)
    add_subdirectory(diagnostics/debugdraw)
    if(GTS_ENABLE_PHYSICS)
        add_subdirectory(diagnostics/physics)
    endif()
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
    "${CMAKE_CURRENT_SOURCE_DIR}/animation/transform"

    # Tweening
    "${CMAKE_CURRENT_SOURCE_DIR}/tween"

    # Transform
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/hierarchy"
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/transform/systems"

)

target_link_libraries(gravitas_modules INTERFACE
    gravitas_core
    gravitas_dialogue
    gravitas_model_loading
    gravitas_model_realization
    gravitas_model_instances
    gravitas_model_frontend
)

foreach(GTS_OPTIONAL_MODULE_TARGET
        gravitas_visualnovel
        gravitas_rendering
        gravitas_physics
        gravitas_debugdraw
        gravitas_diagnostics_physics
        gravitas_tools)
    if(TARGET ${GTS_OPTIONAL_MODULE_TARGET})
        target_link_libraries(gravitas_modules INTERFACE ${GTS_OPTIONAL_MODULE_TARGET})
    endif()
endforeach()
