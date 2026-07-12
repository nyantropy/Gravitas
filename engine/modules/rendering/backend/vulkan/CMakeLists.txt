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
# External translation units — outside the owned directory tree
list(APPEND VULKAN_BACKEND_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../external/stb_image_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../external/stb_image_write_impl.cpp"
)
add_library(gravitas_vulkan_backend STATIC ${VULKAN_BACKEND_SOURCES})

target_compile_features(gravitas_vulkan_backend PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_vulkan_backend PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_vulkan_backend PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_vulkan_backend PUBLIC
    # Backend context shared by Vulkan implementation objects
    "${CMAKE_CURRENT_SOURCE_DIR}/context"

    # Backend-owned graphics module implementation
    "${CMAKE_CURRENT_SOURCE_DIR}/graphics"

    # Rendering pass headers
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering"
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

    # Bundled GLM (needed by vertex types, model loader, texture loader)
    "${GTS_ENGINE_ROOT}/external/glm/glm"

    # Generic render viewport contract
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/viewport"

    # Editor/tool preview render packets
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/preview"

    # Rendering config/constants
    "${CMAKE_CURRENT_SOURCE_DIR}/../../core/init"
)

target_link_libraries(gravitas_vulkan_backend PUBLIC
    gravitas_vulkan_setup
    gravitas_core          # provides core options and GlmConfig
)
