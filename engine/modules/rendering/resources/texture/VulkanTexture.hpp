#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include <stb_image.h>

#include "VulkanLogicalDevice.hpp"
#include "VulkanPhysicalDevice.hpp"

#include "vcsheet.h"
#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"
#include "BufferUtil.hpp"

class VulkanTexture
{
    public:
        VulkanTexture(const std::string path);
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

        void createTextureImageView();
        void createTextureImage(const std::string path);
        void createTextureSampler();
};