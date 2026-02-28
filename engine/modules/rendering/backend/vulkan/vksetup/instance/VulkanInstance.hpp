#pragma once

#include <vulkan/vulkan.h>


#include <array>
#include <iostream>
#include <vector>
#include <cstring>

#include "VulkanInstanceConfig.h"

class VulkanInstance 
{
    public:
        VulkanInstance(VulkanInstanceConfig config);
        ~VulkanInstance();

        VkInstance& getInstance();
        const std::vector<const char*>& getValidationLayers() const;

    private:
        void createInstance();
        bool checkValidationLayerSupport();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        VkInstance instance;
        VulkanInstanceConfig config;

        std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
        VkDebugUtilsMessengerEXT debugMessenger;
};

