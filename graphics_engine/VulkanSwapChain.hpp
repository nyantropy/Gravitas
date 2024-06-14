#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <vector>
#include <limits>
#include <algorithm>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"
#include "GTSOutputWindow.hpp"

class VulkanSwapChain
{
    private:
        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        GTSOutputWindow* vwindow;
        WindowSurface* vsurface;
        VulkanLogicalDevice* vlogicaldevice;
        VulkanPhysicalDevice* vphysicaldevice;        

        void createSwapChain();
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

        void createImageViews();

    public:
        VulkanSwapChain(GTSOutputWindow* vwindow, WindowSurface* vsurface, VulkanPhysicalDevice* vphysicaldevice, VulkanLogicalDevice* vlogicaldevice);
        ~VulkanSwapChain();

        VkSwapchainKHR& getSwapChain();
        std::vector<VkImage>& getSwapChainImages();
        VkFormat& getSwapChainImageFormat();
        VkExtent2D& getSwapChainExtent();
        std::vector<VkImageView>& getSwapChainImageViews();
        std::vector<VkFramebuffer>& getSwapChainFramebuffers();

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};