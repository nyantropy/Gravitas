#pragma once

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <set>

#include "QueueFamilyIndices.h"
#include "SwapChainSupportDetails.h"

#include "VulkanPhysicalDeviceConfig.h"

class VulkanPhysicalDevice 
{
    public:
        // Constructor and Destructor
        VulkanPhysicalDevice(VulkanPhysicalDeviceConfig config);
        ~VulkanPhysicalDevice();

        VkPhysicalDevice& getPhysicalDevice();
        QueueFamilyIndices& getQueueFamilyIndices();
        SwapChainSupportDetails& getSwapChainSupportDetails();
        const std::vector<const char*>& getPhysicalDeviceExtensions();

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    private:
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device, QueueFamilyIndices indices);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        VulkanPhysicalDeviceConfig config;

        //the objects we wrap in this class
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilyIndices;
        SwapChainSupportDetails swapChainSupportDetails;
        
        // physical device extensions
        const std::vector<const char*> physicalDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};