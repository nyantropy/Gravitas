cmake_minimum_required(VERSION 3.20)
project(GravitasEngine VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(GTS_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
option(GTS_BUILD_TEST_SCENES "Build bundled engine validation scenes" ON)
option(GTS_ENABLE_MODULE_SMOKE_TESTS "Register CTest smoke tests for engine module option combinations" ON)
option(GTS_ENABLE_RUNTIME_SMOKE_TESTS "Register CTest runtime smoke tests for engine startup paths" ON)

add_subdirectory(engine)
if(GTS_BUILD_TEST_SCENES AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/applications/CMakeLists.txt")
    add_subdirectory(applications)
endif()

# ── Shader compilation ─────────────────────────────────────────────────────────
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
        COMMAND ${GLSLC} "${SHADER_DIR}/fragmentshader_pbr.frag" -o "${SHADER_DIR}/frag_pbr.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/vertexshader.vert"  -o "${SHADER_DIR}/vert.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/particle_frag.frag" -o "${SHADER_DIR}/particle_frag.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/particle_vert.vert" -o "${SHADER_DIR}/particle_vert.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/particle_mesh_frag.frag" -o "${SHADER_DIR}/particle_mesh_frag.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/particle_mesh_vert.vert" -o "${SHADER_DIR}/particle_mesh_vert.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/ui_frag.frag"       -o "${SHADER_DIR}/ui_frag.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/ui_vert.vert"       -o "${SHADER_DIR}/ui_vert.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/upscale_frag.frag"  -o "${SHADER_DIR}/upscale_frag.spv"
        COMMAND ${GLSLC} "${SHADER_DIR}/upscale_vert.vert"  -o "${SHADER_DIR}/upscale_vert.spv"
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

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(CTest)
    if(BUILD_TESTING AND GTS_ENABLE_MODULE_SMOKE_TESTS)
        include("${GTS_ENGINE_ROOT}/cmake/GravitasModuleSmokeTests.cmake")
    endif()
    if(BUILD_TESTING AND GTS_ENABLE_RUNTIME_SMOKE_TESTS AND TARGET gravitas_vulkan_backend)
        add_subdirectory(tests)
    endif()
endif()
