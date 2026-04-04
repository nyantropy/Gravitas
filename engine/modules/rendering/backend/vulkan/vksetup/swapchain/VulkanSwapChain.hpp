#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <limits>
#include <algorithm>
#include <stdexcept>

#include "VulkanSwapChainConfig.h"
#include "SwapChainSupportDetails.h"
#include "QueueFamilyIndices.h"

class VulkanSwapChain
{
    private:
        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;
        VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        
        VulkanSwapChainConfig config;

        void createSwapChain();
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

        void createImageViews();
        void createFramebuffers();

    public:
        VulkanSwapChain(VulkanSwapChainConfig config);
        ~VulkanSwapChain();

        VkSwapchainKHR& getSwapChain();
        std::vector<VkImage>& getSwapChainImages();
        VkFormat& getSwapChainImageFormat();
        VkExtent2D& getSwapChainExtent();
        std::vector<VkImageView>& getSwapChainImageViews();
        VkPresentModeKHR getPresentMode() const;

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};
