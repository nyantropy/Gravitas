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
        // nearestFilter = true: NEAREST mag/min, no anisotropy, no mip blending.
        // Use for pixel-art / bitmap-font atlases that must not be blurred.
        VulkanTexture(const std::string path, bool nearestFilter = false);
        ~VulkanTexture();

        VkImage& getTextureImage();
        VkDeviceMemory& getTextureImageMemory();
        VkImageView& getTextureImageView();
        VkSampler& getTextureSampler();

    private:
        bool nearestFilter;

        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        void createTextureImageView();
        void createTextureImage(const std::string path);
        void createTextureSampler();
};