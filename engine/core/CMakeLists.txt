# ── gravitas_core ──────────────────────────────────────────────────────────────
# Pure C++ core: ECS framework, input, scene management, options, time, RNG.
# No Vulkan, no GLFW. All sub-directories are header-only except rng/.
file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
add_library(gravitas_core STATIC ${CORE_SOURCES})

target_compile_features(gravitas_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_core PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# GRAVITAS_ENGINE_ROOT / RESOURCES macros (used by GraphicsConstants.h)
target_compile_definitions(gravitas_core PUBLIC
    GRAVITAS_ENGINE_ROOT="${GTS_ENGINE_ROOT}"
    GRAVITAS_ENGINE_RESOURCES="${GTS_ENGINE_ROOT}/resources"
)

target_include_directories(gravitas_core PUBLIC
    # Core root
    "${CMAKE_CURRENT_SOURCE_DIR}"

    # Command buffer
    "${CMAKE_CURRENT_SOURCE_DIR}/command"

    # ECS framework
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/world"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/extractors/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/extractors/render"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/extractors/render/frustumcull"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/extractors/render/frustumcull/components"
    # Event system
    "${CMAKE_CURRENT_SOURCE_DIR}/event"

    # Input
    "${CMAKE_CURRENT_SOURCE_DIR}/input/raw"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/action"
    "${CMAKE_CURRENT_SOURCE_DIR}/input/source"

    # Options / constants / types
    "${CMAKE_CURRENT_SOURCE_DIR}/options"

    # Render description components (StaticMeshComponent, MaterialComponent, etc.)
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/desc"

    # RNG
    "${CMAKE_CURRENT_SOURCE_DIR}/rng"

    # Scene management
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/manager"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/context"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/data"

    # Script (header-only)
    "${CMAKE_CURRENT_SOURCE_DIR}/script/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/script/systems"

    # Time
    "${CMAKE_CURRENT_SOURCE_DIR}/time"

    # UI core model
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"

    # Bundled GLM (GlmConfig.h uses #include <glm.hpp>)
    "${GTS_ENGINE_ROOT}/external/glm/glm"
)
