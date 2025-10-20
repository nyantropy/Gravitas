add_library(
    renderpass

    VulkanRenderPass.hpp
    VulkanRenderPass.cpp

    VulkanRenderPassConfig.h
)

target_link_libraries(renderpass PUBLIC rendering)

target_include_directories(renderpass PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")