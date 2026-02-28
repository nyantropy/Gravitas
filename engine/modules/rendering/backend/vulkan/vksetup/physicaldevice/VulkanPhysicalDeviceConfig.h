#pragma once

#include <vulkan/vulkan.h>

// same story as all other config objects
struct VulkanPhysicalDeviceConfig 
{
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
};