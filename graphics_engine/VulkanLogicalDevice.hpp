#pragma once

#include <vulkan/vulkan.h>
#include "VulkanPhysicalDevice.hpp"


class VulkanLogicalDevice
{
    private:
        VulkanInstance* vinstance;
        VulkanPhysicalDevice* vphysicaldevice;
        bool enableValidationLayers;

        VkDevice device;
        VkQueue graphicsQueue;
        VkQueue presentQueue;

        void createLogicalDevice();
    public:
        VulkanLogicalDevice(VulkanInstance* vinstance, VulkanPhysicalDevice* vphysicaldevice, bool enableValidationLayers);
        ~VulkanLogicalDevice();
        VkDevice& getDevice();
        VkQueue& getGraphicsQueue();
        VkQueue& getPresentQueue();

};