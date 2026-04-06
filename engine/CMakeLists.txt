cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(GravitasEngine VERSION 0.1.0 LANGUAGES C CXX)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    endif()

    set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
    option(GTS_BUILD_TEST_SCENES "Build bundled engine validation scenes" ON)
endif()

if(NOT DEFINED GTS_ENGINE_ROOT)
    get_filename_component(GTS_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

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

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(GTS_BUILD_TEST_SCENES AND EXISTS "${GTS_ENGINE_ROOT}/applications/CMakeLists.txt")
        add_subdirectory("${GTS_ENGINE_ROOT}/applications" "${CMAKE_BINARY_DIR}/applications")
    endif()

    find_program(GLSLC glslc
        HINTS
            "$ENV{VULKAN_SDK}/bin"
            "/usr/bin"
            "/usr/local/bin"
    )

    if(GLSLC)
        set(SHADER_DIR "${GTS_ENGINE_ROOT}/shaders")
        add_custom_target(compile_shaders
            COMMAND ${GLSLC} "${SHADER_DIR}/fragmentshader.frag" -o "${SHADER_DIR}/frag.spv"
            COMMAND ${GLSLC} "${SHADER_DIR}/vertexshader.vert"  -o "${SHADER_DIR}/vert.spv"
            COMMAND ${GLSLC} "${SHADER_DIR}/ui_frag.frag"       -o "${SHADER_DIR}/ui_frag.spv"
            COMMAND ${GLSLC} "${SHADER_DIR}/ui_vert.vert"       -o "${SHADER_DIR}/ui_vert.spv"
            COMMENT "Compiling GLSL shaders to SPIR-V"
            VERBATIM
        )
    else()
        message(WARNING
            "glslc not found — shader recompilation unavailable. "
            "Pre-compiled SPIR-V binaries in shaders/ are used. "
            "Install the Vulkan SDK to enable shader compilation."
        )
    endif()
endif()
