#pragma once

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <set>

#include "QueueFamilyIndices.h"
#include "SwapChainSupportDetails.h"
#include "VulkanInstance.hpp"
#include "WindowSurface.hpp"

class VulkanPhysicalDevice 
{
    public:
        // Constructor and Destructor
        VulkanPhysicalDevice(VulkanInstance* vinstance, WindowSurface* vsurface);
        ~VulkanPhysicalDevice();

        VkPhysicalDevice& getPhysicalDevice();
        QueueFamilyIndices& getQueueFamilyIndices();
        SwapChainSupportDetails& getSwapChainSupportDetails();
        const std::vector<const char*>& getDeviceExtensions();

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    private:
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device, QueueFamilyIndices indices);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        //pointers to relevant classes
        VulkanInstance* vinstance;
        WindowSurface* vsurface;

        //the object we wrap
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilyIndices;
        SwapChainSupportDetails swapChainSupportDetails;
        
        //device extensions
        const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};