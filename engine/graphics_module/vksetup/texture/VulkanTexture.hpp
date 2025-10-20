#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include <stb_image.h>

#include "VulkanLogicalDevice.hpp"
#include "VulkanPhysicalDevice.hpp"

#include "VulkanTextureConfig.h"

#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"
#include "BufferUtil.hpp"

class VulkanTexture
{
    public:
        VulkanTexture(VulkanTextureConfig config);
        ~VulkanTexture();

        VkImage& getTextureImage();
        VkDeviceMemory& getTextureImageMemory();
        VkImageView& getTextureImageView();
        VkSampler& getTextureSampler();

    private:
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        VulkanTextureConfig config;

        void createTextureImageView();
        void createTextureImage();
        void createTextureSampler();
};