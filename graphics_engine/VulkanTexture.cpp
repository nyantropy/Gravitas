#include "VulkanTexture.hpp"

VulkanTexture::VulkanTexture(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, VulkanRenderer* vrenderer, const std::string texture_path)
{
    this->vlogicaldevice = vlogicaldevice;
    this->vphysicaldevice = vphysicaldevice;
    this->vrenderer = vrenderer;
    this->texture_path = texture_path;

    createTextureImage();
    createTextureImageView();
    createTextureSampler();
}

VulkanTexture::~VulkanTexture()
{
    vkDestroyImageView(vlogicaldevice->getDevice(), textureImageView, nullptr);
    vkDestroyImage(vlogicaldevice->getDevice(), textureImage, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), textureImageMemory, nullptr);
    vkDestroySampler(vlogicaldevice->getDevice(), textureSampler, nullptr);
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

void VulkanTexture::createTextureImageView() 
{
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vlogicaldevice->getDevice(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create image view!");
    }
}

void VulkanTexture::createTextureSampler() 
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vphysicaldevice->getPhysicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(vlogicaldevice->getDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void VulkanTexture::createTextureImage() 
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(texture_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) 
    {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice,
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);

    stbi_image_free(pixels);

    vrenderer->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    vrenderer->transitionImageLayout(vlogicaldevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        GtsBufferService::copyBufferToImage(vlogicaldevice, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    vrenderer->transitionImageLayout(vlogicaldevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
}