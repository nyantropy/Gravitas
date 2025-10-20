#pragma once

#include <vector>
#include <memory>
#include <vulkan/vulkan.h>

#include "Renderer.hpp"
#include "RendererConfig.h"

#include "Attachment.hpp"
#include "AttachmentConfig.h"

#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"

#include "FormatUtil.hpp"

class ForwardRenderer : Renderer
{
    private:
        std::unique_ptr<Attachment> depthAttachment;
        std::unique_ptr<VulkanRenderPass> vrenderpass;

        VkFormat findDepthFormat()
        {
            // we can use these variables to find a depth format
            std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            VkFormat depthFormat = FormatUtil::findSupportedFormat(config.vkPhysicalDevice, candidates, tiling, features);

            return depthFormat;
        }

        void createDepthAttachment()
        {
            AttachmentConfig attachConfig;
            // raw vulkan objects
            attachConfig.vkDevice = this->config.vkDevice;
            attachConfig.vkPhysicalDevice = this->config.vkPhysicalDevice;
            attachConfig.vkExtent = this->config.vkExtent;

            // specific for image
            attachConfig.format = findDepthFormat();
            attachConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            attachConfig.imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            // specific for memory
            attachConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            // specific for the image view
            attachConfig.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

            // create a simple depth attachment
            depthAttachment = std::make_unique<Attachment>(attachConfig);
        }

        void createRenderPass()
        {
            VulkanRenderPassConfig vrpConfig;
            vrpConfig.vkDevice = this->config.vkDevice;
            
            // use the depth format we found earlier
            vrpConfig.depthFormat = findDepthFormat();

            // we need to use the swapchainimageformat here
            vrpConfig.colorFormat = this->config.swapChainImageFormat;

            vrenderpass = std::make_unique<VulkanRenderPass>(vrpConfig);
        }

        void createResources() override
        {
            createDepthAttachment();
            createRenderPass();
            


        }
    public:
        ForwardRenderer(RendererConfig config) : Renderer(config)
        {
            createResources();
        }

        ~ForwardRenderer()
        {
            if(vrenderpass) vrenderpass.reset();
            if(depthAttachment) depthAttachment.reset();
        }


                 
        void renderFrame() override
        {

        }
        
        // filler getters, not to be used in the real program
        Attachment* getAttachmentWrapper() { return depthAttachment.get(); }

        VulkanRenderPass* getRenderPassWrapper() { return vrenderpass.get(); }
};