# Layered CPU/model and reusable animation/material services precede rendering.
add_subdirectory(assets)
add_subdirectory(model)
add_subdirectory(rendering/core/material)
add_subdirectory(animation/skeletal)
add_subdirectory(animation/skinning)
add_subdirectory(transform)
add_subdirectory(animation/transform)

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
    add_subdirectory(diagnostics)
endif()
if(GTS_ENABLE_TOOLS)
    add_subdirectory(tools)
endif()

# ── gravitas_modules — INTERFACE aggregator for ECS feature modules ────────────
# Lightweight ECS modules are header-only; compiled feature modules are linked below.
add_library(gravitas_modules INTERFACE)

target_compile_features(gravitas_modules INTERFACE cxx_std_20)

target_link_libraries(gravitas_modules INTERFACE
    gravitas_core
    gravitas_transform
    gravitas_transform_animation
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
