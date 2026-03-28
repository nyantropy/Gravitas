# Core must be processed first — modules and rendering depend on gravitas_core.
add_subdirectory(core)
add_subdirectory(modules)

# ── gravitas_engine — umbrella INTERFACE target ────────────────────────────────
# Applications link against this single target and get everything transitively.
add_library(gravitas_engine INTERFACE)
target_compile_features(gravitas_engine INTERFACE cxx_std_20)

target_link_libraries(gravitas_engine INTERFACE
    gravitas_core
    gravitas_modules
    gravitas_rendering
)

# Engine-root headers (GravitasEngine.hpp, GtsGameLoop.h, GtsPlatform.h, EngineConfig.h)
target_include_directories(gravitas_engine INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Backward-compatibility alias — existing application CMakeLists files use GravitasEngine
add_library(GravitasEngine ALIAS gravitas_engine)
