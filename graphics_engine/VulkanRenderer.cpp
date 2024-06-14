#include "VulkanRenderer.hpp"

VulkanRenderer::VulkanRenderer() 
{
}

VulkanRenderer::~VulkanRenderer() 
{
}

void VulkanRenderer::init(VulkanPhysicalDevice* vphysicaldevice)
{
    this->vphysicaldevice = vphysicaldevice;
}

// void VulkanRenderer::createDepthResources(VkExtent2D extent) {
//     VkFormat depthFormat = findDepthFormat();

//     // Assuming vswapchain is an instance of VulkanSwapchain
//     createImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
//     depthImageView = vswapchain->createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
// }

VkFormat VulkanRenderer::findDepthFormat() 
{
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) 
{
    for (VkFormat format : candidates) 
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vphysicaldevice->getPhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) 
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) 
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}
