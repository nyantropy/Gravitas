#pragma once

#include <vulkan/vulkan.h>
#include <string>

#include <stb_image.h>

#include "VulkanBackendContext.h"
#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"
#include "BufferUtil.hpp"
#include "TextureColorSpace.h"
#include "AssetTypes.h"

class VulkanTexture
{
    public:
        // nearestFilter = true: NEAREST mag/min, no anisotropy, no mip blending.
        // Use for pixel-art / bitmap-font atlases that must not be blurred.
        VulkanTexture(VulkanBackendContext& backendContext,
                      const std::string path,
                      bool nearestFilter = false,
                      bool clampToEdge = false,
                      TextureColorSpace colorSpace = TextureColorSpace::SRgb);
        VulkanTexture(VulkanBackendContext& backendContext,
                      const gts::rendering::TextureAssetData& asset);
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
        VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;
        uint32_t mipLevels = 1;
        gts::rendering::TextureSamplerDesc samplerDesc{};
        VulkanBackendContext& backendContext;
        int width = 0;
        int height = 0;

        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        void createTextureImageView();
        void createTextureImage(const std::string path);
        void createTextureImage(const gts::rendering::TextureAssetData& asset);
        void createTextureSampler();
};
