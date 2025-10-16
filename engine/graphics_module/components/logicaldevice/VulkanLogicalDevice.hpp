#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <set>
#include <stdexcept>

#include "VulkanLogicalDeviceConfig.h"
#include "QueueFamilyIndices.h"

class VulkanLogicalDevice
{
    private:
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;

        VulkanLogicalDeviceConfig config;

        void createLogicalDevice();
        void createCommandPool();
    public:
        VulkanLogicalDevice(VulkanLogicalDeviceConfig config);
        ~VulkanLogicalDevice();
        VkDevice& getDevice();
        VkQueue& getGraphicsQueue();
        VkQueue& getPresentQueue();
        VkCommandPool& getCommandPool();

};