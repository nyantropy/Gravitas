#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <vector>
#include <limits>
#include <algorithm>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"
#include "OutputWindow.hpp"
#include "VulkanRenderer.hpp"
#include "VulkanRenderPass.hpp"

class VulkanRenderer;
class VulkanRenderPass;

class VulkanSwapChain
{
    private:
        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;

        OutputWindow* vwindow;
        VulkanSurface* vsurface;
        VulkanLogicalDevice* vlogicaldevice;
        VulkanPhysicalDevice* vphysicaldevice;
        VulkanRenderer* vrenderer;
        VulkanRenderPass* vrenderpass;        

        void createSwapChain();
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

        void createImageViews();
        void createFramebuffers();

    public:
        VulkanSwapChain(OutputWindow* vwindow, VulkanSurface* vsurface, VulkanPhysicalDevice* vphysicaldevice,
         VulkanLogicalDevice* vlogicaldevice);
        ~VulkanSwapChain();

        VkSwapchainKHR& getSwapChain();
        std::vector<VkImage>& getSwapChainImages();
        VkFormat& getSwapChainImageFormat();
        VkExtent2D& getSwapChainExtent();
        std::vector<VkImageView>& getSwapChainImageViews();

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};