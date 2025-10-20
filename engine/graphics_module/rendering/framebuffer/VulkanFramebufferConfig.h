#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct VulkanFramebufferConfig
{
    VkDevice device = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<std::vector<VkImageView>> attachmentsPerFramebuffer;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 1;
};