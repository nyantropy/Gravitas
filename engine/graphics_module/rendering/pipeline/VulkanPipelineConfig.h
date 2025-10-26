#pragma once

#include <vulkan/vulkan.h>
#include <string>

struct VulkanPipelineConfig 
{    
    // shader paths
    std::string vertexShaderPath;
    std::string fragmentShaderPath;

    VkRenderPass vkRenderPass;
};