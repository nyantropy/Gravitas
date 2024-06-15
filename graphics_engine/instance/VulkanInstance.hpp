#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <array>
#include <iostream>
#include <vector>
#include <cstring>

class VulkanInstance 
{
    public:
        VulkanInstance(bool enableValidationLayers);
        ~VulkanInstance();

        VkInstance& getInstance();
        const std::vector<const char*>& getValidationLayers();

    private:
        void createInstance();
        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        VkInstance instance;
        bool enableValidationLayers;
        const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
};