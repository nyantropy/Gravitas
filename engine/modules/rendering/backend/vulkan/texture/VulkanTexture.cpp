#include "VulkanTexture.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    VkFormat vkFormatForTextureAssetFormat(gts::rendering::TextureAssetFormat format)
    {
        switch (format)
        {
            case gts::rendering::TextureAssetFormat::RGBA8_SRgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case gts::rendering::TextureAssetFormat::RGBA8_UNorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            default:
                throw std::runtime_error("unsupported cooked texture format");
        }
    }

    VkFilter vkFilter(gts::rendering::TextureFilter filter)
    {
        return filter == gts::rendering::TextureFilter::Nearest
            ? VK_FILTER_NEAREST
            : VK_FILTER_LINEAR;
    }

    VkSamplerMipmapMode vkMipmapMode(gts::rendering::TextureMipmapMode mode)
    {
        return mode == gts::rendering::TextureMipmapMode::Nearest
            ? VK_SAMPLER_MIPMAP_MODE_NEAREST
            : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    VkSamplerAddressMode vkAddressMode(gts::rendering::TextureAddressMode mode)
    {
        return mode == gts::rendering::TextureAddressMode::ClampToEdge
            ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
            : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    void createImage2D(VkDevice& device,
                       uint32_t width,
                       uint32_t height,
                       uint32_t mipLevels,
                       VkFormat format,
                       VkImageTiling tiling,
                       VkImageUsageFlags usage,
                       VkImage& image)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");
    }

    void transitionImageLayoutMips(VkDevice& device,
                                   VkCommandPool& commandPool,
                                   VkQueue& graphicsQueue,
                                   VkImage image,
                                   uint32_t mipLevels,
                                   VkImageLayout oldLayout,
                                   VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = BufferUtil::beginSingleTimeCommands(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage,
            destinationStage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        BufferUtil::endSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }

    void copyBufferToImageMips(VkDevice& device,
                               VkCommandPool& commandPool,
                               VkQueue& graphicsQueue,
                               VkBuffer buffer,
                               VkImage image,
                               const std::vector<VkBufferImageCopy>& regions)
    {
        VkCommandBuffer commandBuffer = BufferUtil::beginSingleTimeCommands(device, commandPool);
        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(regions.size()),
            regions.data());
        BufferUtil::endSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }
}

VulkanTexture::VulkanTexture(VulkanBackendContext& backendContext,
                             const std::string path,
                             bool nearestFilter,
                             bool clampToEdge,
                             TextureColorSpace colorSpace)
    : nearestFilter(nearestFilter)
    , clampToEdge(clampToEdge)
    , textureFormat(colorSpace == TextureColorSpace::SRgb
        ? VK_FORMAT_R8G8B8A8_SRGB
        : VK_FORMAT_R8G8B8A8_UNORM)
    , backendContext(backendContext)
{
    samplerDesc.minFilter = nearestFilter
        ? gts::rendering::TextureFilter::Nearest
        : gts::rendering::TextureFilter::Linear;
    samplerDesc.magFilter = samplerDesc.minFilter;
    samplerDesc.mipmapMode = nearestFilter
        ? gts::rendering::TextureMipmapMode::Nearest
        : gts::rendering::TextureMipmapMode::Linear;
    samplerDesc.addressU = (nearestFilter || clampToEdge)
        ? gts::rendering::TextureAddressMode::ClampToEdge
        : gts::rendering::TextureAddressMode::Repeat;
    samplerDesc.addressV = samplerDesc.addressU;
    samplerDesc.addressW = samplerDesc.addressU;
    samplerDesc.maxAnisotropy = nearestFilter ? 1.0f : 0.0f;

    createTextureImage(path);
    createTextureImageView();
    createTextureSampler();
}

VulkanTexture::VulkanTexture(VulkanBackendContext& backendContext,
                             const gts::rendering::TextureAssetData& asset)
    : nearestFilter(asset.defaultSampler.minFilter == gts::rendering::TextureFilter::Nearest &&
                    asset.defaultSampler.magFilter == gts::rendering::TextureFilter::Nearest)
    , clampToEdge(asset.defaultSampler.addressU == gts::rendering::TextureAddressMode::ClampToEdge ||
                  asset.defaultSampler.addressV == gts::rendering::TextureAddressMode::ClampToEdge)
    , textureFormat(vkFormatForTextureAssetFormat(asset.format))
    , mipLevels(asset.mipCount)
    , samplerDesc(asset.defaultSampler)
    , backendContext(backendContext)
{
    createTextureImage(asset);
    createTextureImageView();
    createTextureSampler();
}

VulkanTexture::~VulkanTexture()
{
    vkDestroyImageView(backendContext.device(), textureImageView, nullptr);
    vkDestroyImage(backendContext.device(), textureImage, nullptr);
    vkFreeMemory(backendContext.device(), textureImageMemory, nullptr);
    vkDestroySampler(backendContext.device(), textureSampler, nullptr);
}

VkImage& VulkanTexture::getTextureImage()
{
    return textureImage;
}

VkDeviceMemory& VulkanTexture::getTextureImageMemory()
{
    return textureImageMemory;
}

VkImageView& VulkanTexture::getTextureImageView()
{
    return textureImageView;
}

VkSampler& VulkanTexture::getTextureSampler()
{
    return textureSampler;
}

int VulkanTexture::getWidth() const
{
    return width;
}

int VulkanTexture::getHeight() const
{
    return height;
}

void VulkanTexture::createTextureImageView() 
{
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = textureFormat;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(backendContext.device(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create image view!");
    }
}

void VulkanTexture::createTextureSampler()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(backendContext.physicalDevice(), &properties);

    const VkFilter magFilter = vkFilter(samplerDesc.magFilter);
    const VkFilter minFilter = vkFilter(samplerDesc.minFilter);
    const VkSamplerMipmapMode mipmapMode = vkMipmapMode(samplerDesc.mipmapMode);
    const VkSamplerAddressMode addressU = vkAddressMode(samplerDesc.addressU);
    const VkSamplerAddressMode addressV = vkAddressMode(samplerDesc.addressV);
    const VkSamplerAddressMode addressW = vkAddressMode(samplerDesc.addressW);
    const bool enableAnisotropy =
        samplerDesc.maxAnisotropy <= 0.0f || samplerDesc.maxAnisotropy > 1.0f;
    const float requestedAnisotropy = samplerDesc.maxAnisotropy <= 0.0f
        ? properties.limits.maxSamplerAnisotropy
        : samplerDesc.maxAnisotropy;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    samplerInfo.addressModeU = addressU;
    samplerInfo.addressModeV = addressV;
    samplerInfo.addressModeW = addressW;
    samplerInfo.anisotropyEnable = enableAnisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = enableAnisotropy
        ? std::min(requestedAnisotropy, properties.limits.maxSamplerAnisotropy)
        : 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = mipmapMode;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = mipLevels > 0 ? static_cast<float>(mipLevels - 1u) : 0.0f;

    if (vkCreateSampler(backendContext.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void VulkanTexture::createTextureImage(const std::string path) 
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    width = texWidth;
    height = texHeight;
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * static_cast<VkDeviceSize>(texHeight) * 4;

    // Grayscale font-atlas post-processing:
    // stbi_load expands a 1-channel (grayscale) PNG to RGBA with A=255 always.
    // The fragment shader discards on color.a < 0.1, so without this step the
    // black background would render as opaque black rather than transparent.
    // Solution: use the luminance value as alpha and set RGB to white so the
    // sampled colour is always white with variable opacity (black → transparent,
    // white → opaque white).  Only applied to pixel textures from a grayscale source.
    if (nearestFilter && texChannels == 1)
    {
        stbi_uc* p = pixels;
        for (int i = 0; i < texWidth * texHeight; ++i, p += 4)
        {
            const stbi_uc lum = p[0]; // R == G == B == original gray value
            p[0] = 255;
            p[1] = 255;
            p[2] = 255;
            p[3] = lum;  // alpha = luminance: black → 0 (transparent), white → 255
        }
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    BufferUtil::createBuffer(backendContext.device(), backendContext.physicalDevice(), imageSize,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(backendContext.device(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(backendContext.device(), stagingBufferMemory);

    stbi_image_free(pixels);

    ImageUtil::createImage(backendContext.device(),
    texWidth, texHeight,
    textureFormat, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, textureImage);

    MemoryUtil::allocateImageMemory(backendContext.device(), backendContext.physicalDevice(), textureImage,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImageMemory);

    ImageUtil::transitionImageLayout(backendContext.device(), backendContext.commandPool(), backendContext.graphicsQueue(), textureImage,
    textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        BufferUtil::copyBufferToImage(backendContext.device(), backendContext.commandPool(), backendContext.graphicsQueue(),
        stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    ImageUtil::transitionImageLayout(backendContext.device(), backendContext.commandPool(), backendContext.graphicsQueue(), textureImage,
    textureFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(backendContext.device(), stagingBuffer, nullptr);
    vkFreeMemory(backendContext.device(), stagingBufferMemory, nullptr);
}

void VulkanTexture::createTextureImage(const gts::rendering::TextureAssetData& asset)
{
    if (asset.width == 0 || asset.height == 0 || asset.mipCount == 0 || asset.mips.size() != asset.mipCount)
        throw std::runtime_error("invalid cooked texture asset");

    width = static_cast<int>(asset.width);
    height = static_cast<int>(asset.height);
    mipLevels = asset.mipCount;

    VkDeviceSize imageSize = 0;
    for (const gts::rendering::TextureMipData& mip : asset.mips)
    {
        if (mip.bytes.empty())
            throw std::runtime_error("invalid cooked texture mip payload");
        imageSize += static_cast<VkDeviceSize>(mip.bytes.size());
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    BufferUtil::createBuffer(
        backendContext.device(),
        backendContext.physicalDevice(),
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(asset.mips.size());

    void* data = nullptr;
    vkMapMemory(backendContext.device(), stagingBufferMemory, 0, imageSize, 0, &data);
    auto* destination = static_cast<uint8_t*>(data);
    VkDeviceSize offset = 0;
    for (uint32_t mipIndex = 0; mipIndex < asset.mipCount; ++mipIndex)
    {
        const gts::rendering::TextureMipData& mip = asset.mips[mipIndex];
        std::memcpy(destination + offset, mip.bytes.data(), mip.bytes.size());

        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mipIndex;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {mip.width, mip.height, 1};
        regions.push_back(region);

        offset += static_cast<VkDeviceSize>(mip.bytes.size());
    }
    vkUnmapMemory(backendContext.device(), stagingBufferMemory);

    createImage2D(
        backendContext.device(),
        asset.width,
        asset.height,
        asset.mipCount,
        textureFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        textureImage);

    MemoryUtil::allocateImageMemory(
        backendContext.device(),
        backendContext.physicalDevice(),
        textureImage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImageMemory);

    transitionImageLayoutMips(
        backendContext.device(),
        backendContext.commandPool(),
        backendContext.graphicsQueue(),
        textureImage,
        asset.mipCount,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImageMips(
        backendContext.device(),
        backendContext.commandPool(),
        backendContext.graphicsQueue(),
        stagingBuffer,
        textureImage,
        regions);

    transitionImageLayoutMips(
        backendContext.device(),
        backendContext.commandPool(),
        backendContext.graphicsQueue(),
        textureImage,
        asset.mipCount,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(backendContext.device(), stagingBuffer, nullptr);
    vkFreeMemory(backendContext.device(), stagingBufferMemory, nullptr);
}
