#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct FramebufferManagerConfig
{
    VkRenderPass vkRenderpass;
    VkImageView  attachmentImageView;

    // Set to false for render passes with no depth attachment (e.g. UI overlay).
    // When false, attachmentImageView is ignored.
    bool hasDepthAttachment = true;
};