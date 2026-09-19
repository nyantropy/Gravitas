# ── gravitas_core ──────────────────────────────────────────────────────────────
# Foundational ECS, input, scenes, commands/events, JSON, math, paths, time and utilities.
# No Vulkan, no GLFW.
file(GLOB_RECURSE CORE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
add_library(gravitas_core STATIC ${CORE_SOURCES})

target_compile_features(gravitas_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_core PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# GRAVITAS_ENGINE_ROOT / RESOURCES macros (used by EnginePaths.h)
target_compile_definitions(gravitas_core PUBLIC
    GRAVITAS_ENGINE_ROOT="${GTS_ENGINE_ROOT}"
    GRAVITAS_ENGINE_RESOURCES="${GTS_ENGINE_ROOT}/resources"
)

target_include_directories(gravitas_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/json"
    # Core root
    "${CMAKE_CURRENT_SOURCE_DIR}"

    # Command buffer
    "${CMAKE_CURRENT_SOURCE_DIR}/command"

    # Engine module lifecycle contracts
    "${CMAKE_CURRENT_SOURCE_DIR}/module"

    # ECS framework
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/execution"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/world"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/context"
    # Event system
    "${CMAKE_CURRENT_SOURCE_DIR}/event"

    # Input
    "${CMAKE_CURRENT_SOURCE_DIR}/input/raw"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/binding"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/source"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/snapshot"

    # Engine/project paths and centralized math configuration
    "${CMAKE_CURRENT_SOURCE_DIR}/paths"
    "${CMAKE_CURRENT_SOURCE_DIR}/math"

    # Scene management
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/manager"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/data"

    # Time
    "${CMAKE_CURRENT_SOURCE_DIR}/time"

    # Threading
    "${CMAKE_CURRENT_SOURCE_DIR}/threading"

    # Shared easing/interpolation primitives used by UI animation and VN runtime
    "${CMAKE_CURRENT_SOURCE_DIR}/tween"

    # Bundled GLM (GlmConfig.h uses #include <glm.hpp>)
    "${GTS_ENGINE_ROOT}/external/glm/glm"
)
