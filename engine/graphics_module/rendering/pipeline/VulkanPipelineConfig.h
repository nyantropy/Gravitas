#pragma once

#include <vulkan/vulkan.h>
#include <string>

struct VulkanPipelineConfig 
{
    // core vulkan structures
    VkDevice vkDevice = VK_NULL_HANDLE;
    
    // shader paths
    std::string vertexShaderPath;
    std::string fragmentShaderPath;

    VkDescriptorSetLayout vkDescriptorSetLayout;
    VkRenderPass vkRenderPass;
};