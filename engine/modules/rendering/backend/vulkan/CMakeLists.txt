# gravitas_vulkan_setup must be defined before the backend target.
add_subdirectory(vksetup)

# ── gravitas_vulkan_backend ────────────────────────────────────────────────────
# All Vulkan rendering, resource management, texture loading, and runtime mesh upload.
# Collapses the former per-folder sub-targets into one logical library.
#
# rendering/, resources/, texture/, and util/ are entirely owned by this target.
# external/ lives outside the owned tree — those files are listed explicitly.
file(GLOB_RECURSE VULKAN_BACKEND_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/texture/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/*.cpp"
)
add_library(gravitas_vulkan_backend STATIC ${VULKAN_BACKEND_SOURCES})

target_compile_features(gravitas_vulkan_backend PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_vulkan_backend PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_vulkan_backend PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_vulkan_backend PRIVATE
    # Backend context shared by Vulkan implementation objects
    "${CMAKE_CURRENT_SOURCE_DIR}/context"

    # Backend-owned graphics module implementation
    "${CMAKE_CURRENT_SOURCE_DIR}/graphics"

    # Rendering pass headers
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/skinning"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/attachment"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/framebuffer"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/framegraph"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/framemanager"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/pipeline"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/particles"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/profiling"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/renderpass"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/shader"
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering/ui"

    # Resource managers
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/camera"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/descriptorset"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/environment"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/font"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/mesh"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/skinning"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/ssbo"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/texture"

    # Texture backend
    "${CMAKE_CURRENT_SOURCE_DIR}/texture"

    # Utility headers (all header-only)
    "${CMAKE_CURRENT_SOURCE_DIR}/util"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/buffer_util"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/format_util"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/image_util"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/memory_util"

    # Bundled external headers (stb_image)
    "${GTS_ENGINE_ROOT}/external/stb"

)

target_link_libraries(gravitas_vulkan_backend PRIVATE
    gravitas_vulkan_setup
    gravitas_mesh_loading
    gravitas_model_realization # immutable CPU geometry views for resource caches
    gravitas_core          # provides foundational contracts and GlmConfig
)
