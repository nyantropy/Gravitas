#pragma once

#include <vulkan/vulkan.h>
#include <string>

struct VulkanTextureConfig
{
    VkDevice vkDevice;
    VkPhysicalDevice vkPhysicalDevice;
    VkCommandPool vkCommandPool;
    VkQueue vkGraphicsQueue;

    std::string texture_path;
};