#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "VulkanContext.hpp"

class VulkanBackendContext
{
public:
    explicit VulkanBackendContext(VulkanContext& context)
        : context(context)
    {
    }

    VulkanContext& vulkanContext() const { return context; }

    VkInstance& instance() const { return context.getInstance(); }
    std::vector<const char*> validationLayers() const { return context.getValidationLayers(); }

    VkSurfaceKHR& surface() const { return context.getSurface(); }

    VkPhysicalDevice& physicalDevice() const { return context.getPhysicalDevice(); }
    QueueFamilyIndices& queueFamilyIndices() const { return context.getQueueFamilyIndices(); }
    SwapChainSupportDetails& swapChainSupportDetails() const { return context.getSwapChainSupportDetails(); }
    std::vector<const char*> physicalDeviceExtensions() const { return context.getPhysicalDeviceExtensions(); }

    VkDevice& device() const { return context.getDevice(); }
    VkQueue& graphicsQueue() const { return context.getGraphicsQueue(); }
    VkQueue& presentQueue() const { return context.getPresentQueue(); }
    VkCommandPool& commandPool() const { return context.getCommandPool(); }

    VkSwapchainKHR& swapChain() const { return context.getSwapChain(); }
    std::vector<VkImage>& swapChainImages() const { return context.getSwapChainImages(); }
    VkFormat& swapChainImageFormat() const { return context.getSwapChainImageFormat(); }
    VkExtent2D& swapChainExtent() const { return context.getSwapChainExtent(); }
    std::vector<VkImageView>& swapChainImageViews() const { return context.getSwapChainImageViews(); }
    VkPresentModeKHR presentMode() const { return context.getPresentMode(); }

    const std::vector<VkImage>& frameOutputImages() const { return context.getFrameOutputImages(); }
    const std::vector<VkImageView>& frameOutputImageViews() const { return context.getFrameOutputImageViews(); }
    VkFormat frameOutputFormat() const { return context.getFrameOutputFormat(); }
    VkExtent2D frameOutputExtent() const { return context.getFrameOutputExtent(); }
    VkPresentModeKHR frameOutputPresentMode() const { return context.getFrameOutputPresentMode(); }

    bool hasSurfaceSupport() const { return context.hasSurfaceSupport(); }
    bool isHeadless() const { return context.isHeadless(); }

private:
    VulkanContext& context;
};
