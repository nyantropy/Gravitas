#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct VulkanFramebufferManagerConfig
{
    VkDevice vkDevice;
    VkExtent2D vkExtent;
    VkRenderPass vkRenderpass;
    
    std::vector<VkImageView> swapchainImageViews;
    VkImageView attachmentImageView;
};