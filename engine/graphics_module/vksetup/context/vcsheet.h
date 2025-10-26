#pragma once

#include "VulkanContext.hpp"

// a small sheet to expose important context members in a more global manner
// an access sheet, so to say
namespace vcsheet
{
    inline VulkanContext* glbcontext = nullptr;

    inline void SetContext(VulkanContext* ctx)
    {
        glbcontext = ctx;
    }

    // VulkanInstance
    inline VkInstance& getInstance() { return glbcontext->getInstance(); }
    inline std::vector<const char*> getValidationLayers() { return glbcontext->getValidationLayers(); }

    // VulkanSurface
    inline VkSurfaceKHR& getSurface() { return glbcontext->getSurface(); }

    // VulkanPhysicalDevice
    inline VkPhysicalDevice& getPhysicalDevice() { return glbcontext->getPhysicalDevice(); }
    inline QueueFamilyIndices& getQueueFamilyIndices() { return glbcontext->getQueueFamilyIndices(); }
    inline SwapChainSupportDetails& getSwapChainSupportDetails() { return glbcontext->getSwapChainSupportDetails(); }
    inline std::vector<const char*> getPhysicalDeviceExtensions() { return glbcontext->getPhysicalDeviceExtensions(); }

    // VulkanLogicalDevice
    inline VkDevice& getDevice() { return glbcontext->getDevice(); }
    inline VkQueue& getGraphicsQueue() { return glbcontext->getGraphicsQueue(); }
    inline VkQueue& getPresentQueue() { return glbcontext->getPresentQueue(); }
    inline VkCommandPool& getCommandPool() { return glbcontext->getCommandPool(); }

    // related to VulkanSwapChain
    inline VkSwapchainKHR& getSwapChain() { return glbcontext->getSwapChain(); }
    inline std::vector<VkImage>& getSwapChainImages() { return glbcontext->getSwapChainImages(); }
    inline VkFormat& getSwapChainImageFormat() { return glbcontext->getSwapChainImageFormat(); }
    inline VkExtent2D& getSwapChainExtent() { return glbcontext->getSwapChainExtent(); }
    inline std::vector<VkImageView>& getSwapChainImageViews() { return glbcontext->getSwapChainImageViews(); }
}