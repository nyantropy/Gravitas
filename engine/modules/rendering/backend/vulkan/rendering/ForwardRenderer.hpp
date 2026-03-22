#pragma once

#include <vector>
#include <memory>
#include <vulkan/vulkan.h>

#include "Renderer.hpp"
#include "RendererConfig.h"

#include "Attachment.hpp"
#include "AttachmentConfig.h"

#include "RenderResourceManager.hpp"
#include "RenderCommandExtractor.hpp"
#include "ObjectUBO.h"

#include "GraphicsConstants.h"
#include "FormatUtil.hpp"

#include "FrameManager.hpp"

#include "GtsEvent.hpp"

#include <UICommand.h>
#include "GtsFrameStats.h"

#include "GtsFrameGraph.h"
#include "SceneRenderStage.h"
#include "UiRenderStage.h"

class ForwardRenderer : Renderer
{
    private:
        // Frame-level resources shared across all stages.
        std::unique_ptr<Attachment>            depthAttachment;
        std::unique_ptr<RenderResourceManager> resourceSystem;
        std::unique_ptr<FrameManager>          frameManager;

        // Per-frame stats — populated in renderFrame, provided to the blackboard.
        GtsFrameStats frameStats;

        // Raw pointers into the frame graph — set during buildFrameGraph().
        SceneRenderStage* sceneStage = nullptr;
        UiRenderStage*    uiStage    = nullptr;

        // Depth format — resolved once in createDepthAttachment(), reused by
        // buildFrameGraph() and SceneRenderStage.
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;

        // Frame graph — built once in createResources(), executed every frame.
        // Declared last so it destructs first (stages reference depthAttachment).
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
            std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            return FormatUtil::findSupportedFormat(vcsheet::getPhysicalDevice(), candidates, tiling, features);
        }

        void createDepthAttachment()
        {
            depthFormat = findDepthFormat();
            AttachmentConfig attachConfig;
            attachConfig.format = depthFormat;
            attachConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            attachConfig.imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            attachConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            attachConfig.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthAttachment = std::make_unique<Attachment>(attachConfig);
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
                depthFormat, extent,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED);

            auto ownedScene = std::make_unique<SceneRenderStage>(
                resourceSystem.get(),
                depthAttachment->getImageView(),
                depthFormat,
                swapchainHandle0,
                depthHandle);
            sceneStage = ownedScene.get();
            frameGraph.addStage(std::move(ownedScene));

            auto ownedUi = std::make_unique<UiRenderStage>(
                resourceSystem.get(),
                swapchainHandle0);
            uiStage = ownedUi.get();
            frameGraph.addStage(std::move(ownedUi));

            frameGraph.compile();
        }

        void createResources() override
        {
            createDepthAttachment();
            resourceSystem = std::make_unique<RenderResourceManager>();
            frameManager   = std::make_unique<FrameManager>();
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
            frameGraph.provideData(&frameStats);
            frameGraph.execute(commandBuffer, imageIndex, currentFrame);

            // Triangle count is written by SceneRenderStage during execute — read it back.
            if (sceneStage)
                frameStats.triangleCount = sceneStage->getLastTriangleCount();
                //std::cout << sceneStage->getLastTriangleCount() << std::endl;

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
            if (frameManager)    frameManager.reset();
            if (resourceSystem)  resourceSystem.reset();
            if (depthAttachment) depthAttachment.reset();
        }

        void toggleDebugOverlay() override
        {
            if (uiStage)
                uiStage->getDebugOverlay().setEnabled(!uiStage->getDebugOverlay().isEnabled());
        }

        void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                         const std::vector<UICommandList>& uiLists,
                         const GtsFrameStats& stats) override
        {
            // Store the pre-populated stats from the engine; triangleCount is
            // filled in after frameGraph.execute() inside recordCommandBuffer.
            uint32_t previousTriangleCount = frameStats.triangleCount;
            frameStats = stats;
            frameStats.triangleCount = previousTriangleCount;

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
        RenderResourceManager* getResourceSystem() { return resourceSystem.get(); }
};
