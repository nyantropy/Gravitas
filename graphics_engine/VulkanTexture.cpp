#include "VulkanTexture.hpp"

VulkanTexture::VulkanTexture(VulkanLogicalDevice* vlogicaldevice)
{
    this->vlogicaldevice = vlogicaldevice;
}

VulkanTexture::~VulkanTexture()
{
    vkDestroyImageView(vlogicaldevice->getDevice(), textureImageView, nullptr);
    vkDestroyImage(vlogicaldevice->getDevice(), textureImage, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), textureImageMemory, nullptr);
}