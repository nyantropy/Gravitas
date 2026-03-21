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

#include "VulkanPipelineConfig.h"
#include "VulkanPipeline.hpp"

#include "RenderResourceManager.hpp"
#include "RenderCommandExtractor.hpp"
#include "ObjectUBO.h"

#include "FramebufferManager.hpp"
#include "FramebufferManagerConfig.h"

#include "GraphicsConstants.h"
#include "FormatUtil.hpp"

#include "FrameManager.hpp"

#include "GtsEvent.hpp"

#include "VulkanUiRenderer.hpp"
#include <UICommand.h>

#include "GtsFrameGraph.h"
#include "SceneRenderStage.h"
#include "UiRenderStage.h"

class ForwardRenderer : Renderer
{
    private:
        // render structs
        std::unique_ptr<Attachment> depthAttachment;
        std::unique_ptr<VulkanRenderPass> vrenderpass;
        std::unique_ptr<VulkanPipeline> vpipeline;
        std::unique_ptr<FramebufferManager> frameBufferManager;
        std::unique_ptr<FrameManager> frameManager;

        // resource system of the renderer
        std::unique_ptr<RenderResourceManager> resourceSystem;

        // UI overlay renderer (declared after resourceSystem so it is destroyed first)
        std::unique_ptr<VulkanUiRenderer> uiRenderer;

        // Frame graph — built once in createResources(), executed every frame.
        GtsFrameGraph frameGraph;

        // misc variables we need for the draw loop
        float FPS = 30.0f;
        double frameInterval = 1.0 / FPS;
        double lastFrameTime = 0.0;
        int frameCounter = 0;

        uint32_t currentFrame = 0;
        bool framebufferResized = false;


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

        void createFrameBuffers()
        {
            FramebufferManagerConfig vfmConfig;
            vfmConfig.attachmentImageView = depthAttachment->getImageView();
            vfmConfig.vkRenderpass = vrenderpass->getRenderPass();
            frameBufferManager = std::make_unique<FramebufferManager>(vfmConfig);
        }

        void buildFrameGraph()
        {
            const auto& images     = vcsheet::getSwapChainImages();
            const auto& imageViews = vcsheet::getSwapChainImageViews();
            const auto  format     = vcsheet::getSwapChainImageFormat();
            const auto  extent     = vcsheet::getSwapChainExtent();

            GtsResourceHandle swapchainHandle0 = GTS_INVALID_RESOURCE;
            for (uint32_t i = 0; i < static_cast<uint32_t>(images.size()); ++i)
            {
                GtsResourceHandle h = frameGraph.importResource(
                    "swapchain_" + std::to_string(i),
                    images[i], imageViews[i],
                    format, extent,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED);
                frameGraph.registerSwapchainHandle(i, h);
                if (i == 0) swapchainHandle0 = h;
            }

            GtsResourceHandle depthHandle = frameGraph.importResource(
                "depth",
                depthAttachment->getImage(),
                depthAttachment->getImageView(),
                findDepthFormat(), extent,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED);

            frameGraph.addStage(std::make_unique<SceneRenderStage>(
                vrenderpass.get(), vpipeline.get(),
                frameBufferManager.get(), resourceSystem.get(),
                swapchainHandle0, depthHandle));

            frameGraph.addStage(std::make_unique<UiRenderStage>(
                uiRenderer.get(), resourceSystem.get(),
                swapchainHandle0));

            frameGraph.compile();
        }

        void createResources() override
        {
            createDepthAttachment();
            createRenderPass();
            resourceSystem = std::make_unique<RenderResourceManager>();
            createPipeline();
            createFrameBuffers();
            frameManager  = std::make_unique<FrameManager>();
            uiRenderer    = std::make_unique<VulkanUiRenderer>();
            buildFrameGraph();
        }

        void recordCommandBuffer(const std::vector<RenderCommand>& renderList,
                                 const std::vector<UICommandList>& uiLists,
                                 VkCommandBuffer commandBuffer, uint32_t imageIndex)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            frameGraph.provideData(&renderList);
            frameGraph.provideData(&uiLists);
            frameGraph.execute(commandBuffer, imageIndex, currentFrame);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        }

        
    public:
        // render events
        GtsEvent<int, uint32_t> onFrameEnded;


        ForwardRenderer(RendererConfig config) : Renderer(config)
        {
            createResources();
        }

        ~ForwardRenderer()
        {
            if(frameManager) frameManager.reset();
            if(frameBufferManager) frameBufferManager.reset();
            if(vpipeline) vpipeline.reset();
            if(vrenderpass) vrenderpass.reset();
            if(depthAttachment) depthAttachment.reset();
        }
              
        void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                         const std::vector<UICommandList>& uiLists) override
        {
            lastFrameTime += dt;

            FrameResources& frame = frameManager->getFrame(currentFrame);

            // Wait for this frame's fence
            vkWaitForFences(vcsheet::getDevice(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

            // Acquire swapchain image
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(vcsheet::getDevice(),
                                                    vcsheet::getSwapChain(),
                                                    UINT64_MAX,
                                                    frame.imageAvailableSemaphore,
                                                    VK_NULL_HANDLE,
                                                    &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            { 
                //recreateSwapChain();
                return; 
            }

            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                throw std::runtime_error("failed to acquire swap chain image!");

            VkFence imageFence = frameManager->getImageFence(imageIndex);
            if (frameManager->getImagesInFlightCount() > imageIndex && imageFence != VK_NULL_HANDLE) 
            {
                vkWaitForFences(vcsheet::getDevice(), 1, &imageFence, VK_TRUE, UINT64_MAX);
            }

            // Write each object's model matrix into its SSBO slot for this frame.
            // The renderer owns the ObjectUBO packing; the ECS only supplies a plain glm::mat4.
            for (const auto& cmdData : renderList)
            {
                ObjectUBO ubo;
                ubo.model = cmdData.modelMatrix;
                resourceSystem->writeObjectSlot(currentFrame, cmdData.objectSSBOSlot, ubo);
            }

            // Reset & record command buffer
            vkResetFences(vcsheet::getDevice(), 1, &frame.inFlightFence);
            vkResetCommandBuffer(frame.commandBuffer, 0);
            recordCommandBuffer(renderList, uiLists, frame.commandBuffer, imageIndex);

            // Submit command buffer
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { frame.imageAvailableSemaphore };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &frame.commandBuffer;

            VkSemaphore signalSemaphores[] = { frameManager->getRenderFinishedSemaphore(imageIndex) };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(vcsheet::getGraphicsQueue(), 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            if (frameManager->getImagesInFlightCount() > imageIndex) 
            {
                frameManager->setImageFence(imageIndex, frame.inFlightFence);
            }

            // fire the on frame ended event, currently non functional
            if (lastFrameTime >= frameInterval) 
            {
                onFrameEnded.notify(dt, imageIndex);
                lastFrameTime -= frameInterval;
                frameCounter++;
            }

            // Present
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            VkSwapchainKHR swapChains[] = { vcsheet::getSwapChain() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            result = vkQueuePresentKHR(vcsheet::getPresentQueue(), &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) 
            {
                framebufferResized = false;
                //recreateSwapChain();
            }
            else if (result != VK_SUCCESS)
            {
                throw std::runtime_error("failed to present swap chain image!");
            }

            currentFrame = (currentFrame + 1) % GraphicsConstants::MAX_FRAMES_IN_FLIGHT;
        }
        
        // filler getters, not to be used in the real program
        Attachment* getAttachmentWrapper() { return depthAttachment.get(); }
        VulkanRenderPass* getRenderPassWrapper() { return vrenderpass.get(); }
        RenderResourceManager* getResourceSystem() { return resourceSystem.get(); }
        VulkanPipeline* getPipeline() { return vpipeline.get(); }
        FramebufferManager* getFrameBufferManager() { return frameBufferManager.get(); }

        //IResourceProvider* getResourceProvider() { return resourceSystem.get(); }
};
