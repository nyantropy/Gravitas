#pragma once

#include <vulkan/vulkan.h>

#include "VulkanLogicalDevice.hpp"

class VulkanTexture
{
    public:
        VulkanTexture(VulkanLogicalDevice* vlogicaldevice);
        ~VulkanTexture();

    private:
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;

        VulkanLogicalDevice* vlogicaldevice;


};