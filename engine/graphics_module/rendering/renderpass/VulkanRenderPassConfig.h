#pragma once

#include <vulkan/vulkan.h>

struct VulkanRenderPassConfig
{
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
};