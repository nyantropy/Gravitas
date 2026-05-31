#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct FramebufferManagerConfig
{
    VkRenderPass vkRenderpass;
    VkImageView  attachmentImageView;
    std::vector<VkImageView> colorImageViews;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t framebufferCount = 0;

    // Set to false for render passes with no depth attachment (e.g. UI overlay).
    // When false, attachmentImageView is ignored.
    bool hasDepthAttachment = true;
};
