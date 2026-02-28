#pragma once

#include <vulkan/vulkan.h>

struct AttachmentConfig 
{
    // image 
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageTiling tiling;
    VkImageUsageFlags imageUsageFlags;

    // memory
    VkMemoryPropertyFlags memoryPropertyFlags;

    // image view
    VkImageAspectFlags imageAspectFlags;
};