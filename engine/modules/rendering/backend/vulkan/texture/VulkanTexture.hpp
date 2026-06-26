#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include <stb_image.h>

#include "VulkanBackendContext.h"
#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"
#include "BufferUtil.hpp"

class VulkanTexture
{
    public:
        // nearestFilter = true: NEAREST mag/min, no anisotropy, no mip blending.
        // Use for pixel-art / bitmap-font atlases that must not be blurred.
        VulkanTexture(VulkanBackendContext& backendContext,
                      const std::string path,
                      bool nearestFilter = false,
                      bool clampToEdge = false);
        ~VulkanTexture();

        VkImage& getTextureImage();
        VkDeviceMemory& getTextureImageMemory();
        VkImageView& getTextureImageView();
        VkSampler& getTextureSampler();
        int getWidth() const;
        int getHeight() const;

    private:
        bool nearestFilter;
        bool clampToEdge;
        VulkanBackendContext& backendContext;
        int width = 0;
        int height = 0;

        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        void createTextureImageView();
        void createTextureImage(const std::string path);
        void createTextureSampler();
};
