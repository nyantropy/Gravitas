add_library(
    vkshader

    VulkanShader.hpp
    VulkanShader.cpp

    VulkanShaderConfig.h
)

target_link_libraries(vkshader PUBLIC rendering)

target_include_directories(vkshader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")