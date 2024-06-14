#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <vector>
#include <limits>
#include <algorithm>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"

class VulkanSwapChain
{
    private:
        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        GLFWwindow* window;
        WindowSurface* vsurface;
        VulkanLogicalDevice* vlogicaldevice;
        VulkanPhysicalDevice* vphysicaldevice;        

        void createSwapChain();
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    public:
        VulkanSwapChain(GLFWwindow* window, WindowSurface* vsurface, VulkanPhysicalDevice* vphysicaldevice, VulkanLogicalDevice* vlogicaldevice);
        //~VulkanSwapChain();
};