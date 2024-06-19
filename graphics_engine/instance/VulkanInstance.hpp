#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <array>
#include <iostream>
#include <vector>
#include <cstring>

#include "GTSOutputWindow.hpp"

class VulkanInstance 
{
    public:
        VulkanInstance(bool enableValidationLayers, GTSOutputWindow* vwindow);
        ~VulkanInstance();

        VkInstance& getInstance();
        const std::vector<const char*>& getValidationLayers();

    private:
        void createInstance();
        bool checkValidationLayerSupport();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

        VkInstance instance;
        bool enableValidationLayers;
        GTSOutputWindow* vwindow;
};