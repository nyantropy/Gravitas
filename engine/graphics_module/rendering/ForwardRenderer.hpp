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

#include "DescriptorSetManager.hpp"
#include "MeshManager.hpp"
#include "UniformBufferManager.hpp"
#include "TextureManager.hpp"

#include "GraphicsConstants.h"
#include "FormatUtil.hpp"

class ForwardRenderer : Renderer
{
    private:
        // render structs
        std::unique_ptr<Attachment> depthAttachment;
        std::unique_ptr<VulkanRenderPass> vrenderpass;

        // render resource manager classes
        std::unique_ptr<DescriptorSetManager> descriptorSetManager;
        //std::unique_ptr<MeshManager> meshManager;
        //std::unique_ptr<UniformBufferManager> uniformBufferManager;
        //std::unique_ptr<TextureManager> textureManager;

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
            
            // use the depth format we found earlier
            vrpConfig.depthFormat = findDepthFormat();

            // we need to use the swapchainimageformat here
            vrpConfig.colorFormat = vcsheet::getSwapChainImageFormat();

            vrenderpass = std::make_unique<VulkanRenderPass>(vrpConfig);
        }

        // create all the resource managers the renderer needs
        void createResourceManagers()
        {
            // first we have the descriptor set manager
            descriptorSetManager = std::make_unique<DescriptorSetManager>(GraphicsConstants::MAX_FRAMES_IN_FLIGHT, 1000);
            dssheet::SetManager(descriptorSetManager.get());
            
            // and then we also have mesh/texture and uniformbuffer managers, which have less overhead
            //meshManager = std::make_unique<MeshManager>();
            //uniformBufferManager = std::make_unique<UniformBufferManager>();
            //textureManager = std::make_unique<TextureManager>();
        }

        void createResources() override
        {
            createDepthAttachment();
            createRenderPass();
            createResourceManagers();
            


        }
    public:
        ForwardRenderer(RendererConfig config) : Renderer(config)
        {
            createResources();
        }

        ~ForwardRenderer()
        {
            //if(meshManager) meshManager.reset();
            //if(uniformBufferManager) uniformBufferManager.reset();
            //if(textureManager) textureManager.reset();
            if(descriptorSetManager) descriptorSetManager.reset();
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