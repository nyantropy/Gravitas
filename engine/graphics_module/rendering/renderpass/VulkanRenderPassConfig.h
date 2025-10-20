#pragma once

#include <vulkan/vulkan.h>

struct VulkanRenderPassConfig
{
    VkDevice vkDevice = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
};