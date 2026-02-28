#pragma once

#include <vulkan/vulkan.h>

#include "QueueFamilyIndices.h"

// same story as all other config objects
struct VulkanLogicalDeviceConfig 
{
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;
    std::vector<const char*> physicalDeviceExtensions;

    bool enableValidationLayers;
    std::vector<const char*> vkInstanceValidationLayers;
};