#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct FramebufferManagerConfig
{
    VkRenderPass vkRenderpass;
    VkImageView attachmentImageView;
};