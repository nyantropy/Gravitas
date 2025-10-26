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

#include "ResourceSystem.hpp"
#include "RenderSystem.hpp"

#include "GraphicsConstants.h"
#include "FormatUtil.hpp"

class ForwardRenderer : Renderer
{
    private:
        // render structs
        std::unique_ptr<Attachment> depthAttachment;
        std::unique_ptr<VulkanRenderPass> vrenderpass;
        std::unique_ptr<VulkanPipeline> vpipeline;

        // resource system of the renderer
        std::unique_ptr<ResourceSystem> resourceSystem;

        // render system of the renderer
        //std::unique_ptr<RenderSystem> renderSystem;

        VkFormat findDepthFormat()
        {
            // we can use these variables to find a depth format
            std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            VkFormat depthFormat = FormatUtil::findSupportedFormat(vcsheet::getPhysicalDevice(), candidates, tiling, features);

            return depthFormat;
        }

        void createDepthAttachment()
        {
            AttachmentConfig attachConfig;
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
            vrpConfig.depthFormat = findDepthFormat();
            vrpConfig.colorFormat = vcsheet::getSwapChainImageFormat();
            vrenderpass = std::make_unique<VulkanRenderPass>(vrpConfig);
        }

        void createPipeline()
        {
            VulkanPipelineConfig vpConfig;
            vpConfig.fragmentShaderPath = GraphicsConstants::F_SHADER_PATH;
            vpConfig.vertexShaderPath = GraphicsConstants::V_SHADER_PATH;
            vpConfig.vkRenderPass = vrenderpass->getRenderPass();
            vpipeline = std::make_unique<VulkanPipeline>(vpConfig);
        }

        void createResources() override
        {
            createDepthAttachment();
            createRenderPass();
            resourceSystem = std::make_unique<ResourceSystem>();
            createPipeline();
        }
    public:
        ForwardRenderer(RendererConfig config) : Renderer(config)
        {
            createResources();
        }

        ~ForwardRenderer()
        {
            if(vpipeline) vpipeline.reset();
            if(vrenderpass) vrenderpass.reset();
            if(depthAttachment) depthAttachment.reset();
        }


                 
        void renderFrame() override
        {

        }
        
        // filler getters, not to be used in the real program
        Attachment* getAttachmentWrapper() { return depthAttachment.get(); }
        VulkanRenderPass* getRenderPassWrapper() { return vrenderpass.get(); }
        ResourceSystem* getResourceSystem() { return resourceSystem.get(); }
        VulkanPipeline* getPipeline() { return vpipeline.get(); }
};