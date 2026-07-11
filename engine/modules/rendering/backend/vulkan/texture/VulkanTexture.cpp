#include "VulkanTexture.hpp"

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
    createTextureImage(path);
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
    viewInfo.subresourceRange.levelCount = 1;
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

    const VkFilter            filter      = nearestFilter ? VK_FILTER_NEAREST            : VK_FILTER_LINEAR;
    const VkSamplerMipmapMode mipmapMode  = nearestFilter ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    const VkSamplerAddressMode addrMode =
        (nearestFilter || clampToEdge) ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addrMode;
    samplerInfo.addressModeV = addrMode;
    samplerInfo.addressModeW = addrMode;
    samplerInfo.anisotropyEnable = nearestFilter ? VK_FALSE : VK_TRUE;
    samplerInfo.maxAnisotropy = nearestFilter ? 1.0f : properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = mipmapMode;

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
