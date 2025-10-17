#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "Renderer.hpp"
#include "RendererConfig.h"

#include "Attachment.hpp"
#include "AttachmentConfig.h"

#include "FormatUtil.hpp"

class ForwardRenderer : Renderer
{
    public:
        Attachment att;

        ForwardRenderer(RendererConfig config) : Renderer(config)
        {

        }

        void createResources() override
        {
            // we can use these variables to find a depth format
            std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            VkFormat depthFormat = FormatUtil::findSupportedFormat(config.vkPhysicalDevice, candidates, tiling, features);

            AttachmentConfig attachConfig;
            // raw vulkan objects
            attachConfig.vkDevice = this->config.vkDevice;
            attachConfig.vkPhysicalDevice = this->config.vkPhysicalDevice;
            attachConfig.vkExtent = this->config.vkExtent;

            // specific for image
            attachConfig.format = depthFormat;
            attachConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            attachConfig.imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            // specific for memory
            attachConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            att = Attachment(attachConfig);
        }         
        void renderFrame() override
        {

        }      
};