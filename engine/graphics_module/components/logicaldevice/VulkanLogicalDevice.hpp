#pragma once

#include <vulkan/vulkan.h>
#include "VulkanPhysicalDevice.hpp"

#include "VulkanInstance.hpp"


class VulkanLogicalDevice
{
    private:
        VulkanInstance* vinstance;
        VulkanPhysicalDevice* vphysicaldevice;
        bool enableValidationLayers;

        VkDevice device;
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        VkCommandPool commandPool;

        void createLogicalDevice();
        void createCommandPool();
    public:
        VulkanLogicalDevice(VulkanInstance* vinstance, VulkanPhysicalDevice* vphysicaldevice, bool enableValidationLayers);
        ~VulkanLogicalDevice();
        VkDevice& getDevice();
        VkQueue& getGraphicsQueue();
        VkQueue& getPresentQueue();
        VkCommandPool& getCommandPool();

};