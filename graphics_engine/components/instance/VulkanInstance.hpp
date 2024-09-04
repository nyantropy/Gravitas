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
        VulkanInstance(bool enableValidationLayers, const std::vector<const char*>& extensions);
        ~VulkanInstance();

        VkInstance& getInstance();
        const std::vector<const char*>& getValidationLayers() const;

    private:
        void createInstance(const std::vector<const char*>& extensions);
        bool checkValidationLayerSupport();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        VkInstance instance;
        bool enableValidationLayers;
        std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
        VkDebugUtilsMessengerEXT debugMessenger;
};

