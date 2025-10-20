#pragma once

#include <vulkan/vulkan.h>

#include "OutputWindow.hpp"
#include "SwapChainSupportDetails.h"
#include "QueueFamilyIndices.h"

// same story as all other config objects
struct VulkanSwapChainConfig 
{
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    VkDevice vkDevice = VK_NULL_HANDLE;
    SwapChainSupportDetails swapChainSupportDetails;
    QueueFamilyIndices queueFamilyIndices;
    OutputWindow* outputWindowPtr = nullptr;
};