#pragma once

#include <chrono>
#include <memory>
#include <vector>
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
#include "ScreenshotManager.hpp"

#include "GtsEventBus.hpp"
#include "GtsEventTypes.h"

#include <UiCommand.h>
#include "GtsFrameStats.h"

#include "GtsFrameGraph.h"
#include "SceneRenderStage.h"
#include "UiRenderStage.h"
#include "output/HeadlessFrameOutputTarget.hpp"
#include "output/WindowedFrameOutputTarget.hpp"

class ForwardRenderer : Renderer
{
    private:
        // Frame-level resources shared across all stages.
        std::unique_ptr<Attachment>            depthAttachment;
        std::unique_ptr<IFrameOutputTarget>    frameOutputTarget;
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
        bool screenshotRequested = false;
        uint32_t screenshotsTaken = 0;
        uint32_t renderedFrameCount = 0;
        uint32_t autoScreenshotDelayFrames = 0;
        uint32_t diagnosticFramesLogged = 0;
        float secondsSinceLastScreenshot = 1000.0f;
        bool maxScreenshotWarningLogged = false;
        bool minIntervalWarningLogged = false;
        ScreenshotManager screenshotManager;
        GtsEventBus& eventBus;


        VkFormat findDepthFormat()
        {
            std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            return FormatUtil::findSupportedFormat(vcsheet::getPhysicalDevice(), candidates, tiling, features);
        }

        VkFormat findHeadlessColorFormat()
        {
            const std::vector<VkFormat> candidates = {
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_FORMAT_B8G8R8A8_SRGB,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FORMAT_B8G8R8A8_UNORM
            };
            return FormatUtil::findSupportedFormat(
                vcsheet::getPhysicalDevice(),
                candidates,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
        }

        void createFrameOutputTarget()
        {
            if (config.headless)
            {
                const VkFormat colorFormat = findHeadlessColorFormat();
                frameOutputTarget = std::make_unique<HeadlessFrameOutputTarget>(
                    config.renderWidth,
                    config.renderHeight,
                    colorFormat);
            }
            else
            {
                frameOutputTarget = std::make_unique<WindowedFrameOutputTarget>();
            }

            vcsheet::getContext()->registerFrameOutput(
                frameOutputTarget->getImages(),
                frameOutputTarget->getImageViews(),
                frameOutputTarget->getFormat(),
                frameOutputTarget->getExtent(),
                frameOutputTarget->getPresentMode());
        }

        void createDepthAttachment()
        {
            depthFormat = findDepthFormat();
            AttachmentConfig attachConfig;
            attachConfig.width = frameOutputTarget->getExtent().width;
            attachConfig.height = frameOutputTarget->getExtent().height;
            attachConfig.format = depthFormat;
            attachConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            attachConfig.imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            attachConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            attachConfig.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthAttachment = std::make_unique<Attachment>(attachConfig);
        }

        void buildFrameGraph()
        {
            const auto& images     = frameOutputTarget->getImages();
            const auto& imageViews = frameOutputTarget->getImageViews();
            const auto  format     = frameOutputTarget->getFormat();
            const auto  extent     = frameOutputTarget->getExtent();

            GtsResourceHandle outputHandle0 = GTS_INVALID_RESOURCE;
            for (uint32_t i = 0; i < static_cast<uint32_t>(images.size()); ++i)
            {
                GtsResourceHandle h = frameGraph.importResource(
                    "frame_output_" + std::to_string(i),
                    images[i], imageViews[i],
                    format, extent,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED);
                frameGraph.registerSwapchainHandle(i, h);
                if (i == 0) outputHandle0 = h;
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
                outputHandle0,
                depthHandle,
                frameOutputTarget->getSceneFinalLayout());
            sceneStage = ownedScene.get();
            frameGraph.addStage(std::move(ownedScene));

            auto ownedUi = std::make_unique<UiRenderStage>(
                resourceSystem.get(),
                outputHandle0,
                &frameStats,
                false,
                frameOutputTarget->getUiInitialLayout(),
                frameOutputTarget->getUiFinalLayout());
            uiStage = ownedUi.get();
            frameGraph.addStage(std::move(ownedUi));

            frameGraph.compile();
        }

        void createResources() override
        {
            createFrameOutputTarget();
            createDepthAttachment();
            resourceSystem = std::make_unique<RenderResourceManager>();
            frameManager   = std::make_unique<FrameManager>(frameOutputTarget->getImages().size());
            buildFrameGraph();
        }

        bool tryConsumeScreenshotRequest(float dt)
        {
            secondsSinceLastScreenshot += dt;

            if (!screenshotRequested)
                return false;

            if (screenshotsTaken >= config.maxScreenshotsPerRun)
            {
                if (!maxScreenshotWarningLogged)
                {
                    std::cout << "Screenshot request ignored: maxScreenshotsPerRun reached." << std::endl;
                    maxScreenshotWarningLogged = true;
                }
                screenshotRequested = false;
                return false;
            }

            if (secondsSinceLastScreenshot < config.minSecondsBetweenScreenshots)
            {
                if (!minIntervalWarningLogged)
                {
                    std::cout << "Screenshot request ignored: minSecondsBetweenScreenshots not met." << std::endl;
                    minIntervalWarningLogged = true;
                }
                screenshotRequested = false;
                return false;
            }

            minIntervalWarningLogged = false;
            return true;
        }

        void logHeadlessFrameDiagnostics(const std::vector<RenderCommand>& renderList) const
        {
            if (!config.headless)
                return;

            std::cout
                << "[headless] frame=" << renderedFrameCount
                << " renderCommands=" << renderList.size()
                << " visibleObjects=" << frameStats.visibleObjects
                << " totalObjects=" << frameStats.totalObjects
                << " drawCalls=" << frameStats.drawCalls
                << " triangles=" << frameStats.triangleCount
                << std::endl;
        }

        void recordCommandBuffer(const std::vector<RenderCommand>& renderList,
                                 const UiCommandBuffer& uiBuffer,
                                 VkCommandBuffer commandBuffer, uint32_t imageIndex)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            frameGraph.provideData(&renderList);
            frameGraph.provideData(&uiBuffer);
            frameGraph.provideData(&frameStats);
            frameGraph.execute(commandBuffer, imageIndex, currentFrame);

            // Render stats are written by SceneRenderStage during execute — read them back.
            if (sceneStage)
            {
                frameStats.triangleCount    = sceneStage->getLastTriangleCount();
                frameStats.drawCalls        = sceneStage->getLastDrawCalls();
                frameStats.pipelineSwitches = sceneStage->getLastPipelineSwitches();
                frameStats.textureSwitches  = sceneStage->getLastTextureSwitches();
            }

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        }


    public:
        ForwardRenderer(RendererConfig config, GtsEventBus& eventBus)
            : Renderer(config), eventBus(eventBus)
        {
            screenshotRequested = config.captureScreenshotOnFirstFrame;
            autoScreenshotDelayFrames = config.autoScreenshotDelayFrames;
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
                uiStage->getDebugOverlay().toggle();
        }

        void requestScreenshot() override
        {
            screenshotRequested = true;
        }

        void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                         const UiCommandBuffer& uiBuffer,
                         const GtsFrameStats& stats) override
        {
            const auto frameStart = std::chrono::steady_clock::now();
            renderedFrameCount += 1;

            if (!screenshotRequested
                && autoScreenshotDelayFrames > 0
                && renderedFrameCount >= autoScreenshotDelayFrames)
            {
                screenshotRequested = true;
                autoScreenshotDelayFrames = 0;
            }

            // Store the pre-populated stats from the engine; triangleCount is
            // filled in after frameGraph.execute() inside recordCommandBuffer.
            uint32_t previousTriangleCount = frameStats.triangleCount;
            frameStats = stats;
            frameStats.triangleCount = previousTriangleCount;
            frameStats.backendPresentMode = static_cast<uint32_t>(frameOutputTarget->getPresentMode());

            lastFrameTime += dt;

            FrameResources& frame = frameManager->getFrame(currentFrame);

            // Wait for this frame's fence
            const auto fenceWaitStart = std::chrono::steady_clock::now();
            vkWaitForFences(vcsheet::getDevice(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
            const auto fenceWaitEnd = std::chrono::steady_clock::now();
            frameStats.backendFenceWaitCpuMs =
                std::chrono::duration<float, std::milli>(fenceWaitEnd - fenceWaitStart).count();

            // Acquire frame output image
            const auto acquireStart = std::chrono::steady_clock::now();
            FrameOutputBeginResult beginResult =
                frameOutputTarget->beginFrame(currentFrame, frame.imageAvailableSemaphore);
            const auto acquireEnd = std::chrono::steady_clock::now();
            frameStats.backendAcquireCpuMs =
                std::chrono::duration<float, std::milli>(acquireEnd - acquireStart).count();

            if (beginResult.imageIndex == UINT32_MAX)
                return;

            const uint32_t imageIndex = beginResult.imageIndex;

            VkFence imageFence = frameManager->getImageFence(imageIndex);
            if (frameManager->getImagesInFlightCount() > imageIndex && imageFence != VK_NULL_HANDLE)
            {
                const auto imageWaitStart = std::chrono::steady_clock::now();
                vkWaitForFences(vcsheet::getDevice(), 1, &imageFence, VK_TRUE, UINT64_MAX);
                const auto imageWaitEnd = std::chrono::steady_clock::now();
                frameStats.backendImageWaitCpuMs =
                    std::chrono::duration<float, std::milli>(imageWaitEnd - imageWaitStart).count();
            }

            // Write each object's model matrix into its SSBO slot for this frame.
            // The renderer owns the ObjectUBO packing; the ECS only supplies a plain glm::mat4.
            const auto objectWriteStart = std::chrono::steady_clock::now();
            frameStats.backendObjectWrites = 0;
            frameStats.backendObjectWritesSkipped = 0;
            for (const auto& cmdData : renderList)
            {
                ObjectUBO ubo;
                ubo.model = cmdData.modelMatrix;
                if (resourceSystem->writeObjectSlot(currentFrame, cmdData.objectSSBOSlot, ubo))
                    frameStats.backendObjectWrites += 1;
                else
                    frameStats.backendObjectWritesSkipped += 1;
            }
            resourceSystem->flushObjectSSBO(currentFrame);
            const auto objectWriteEnd = std::chrono::steady_clock::now();
            frameStats.backendObjectWriteCpuMs =
                std::chrono::duration<float, std::milli>(objectWriteEnd - objectWriteStart).count();

            // Reset & record command buffer
            const auto fenceResetStart = std::chrono::steady_clock::now();
            vkResetFences(vcsheet::getDevice(), 1, &frame.inFlightFence);
            const auto fenceResetEnd = std::chrono::steady_clock::now();
            frameStats.backendFenceResetCpuMs =
                std::chrono::duration<float, std::milli>(fenceResetEnd - fenceResetStart).count();

            const auto cmdResetStart = std::chrono::steady_clock::now();
            vkResetCommandBuffer(frame.commandBuffer, 0);
            const auto cmdResetEnd = std::chrono::steady_clock::now();
            frameStats.backendCmdResetCpuMs =
                std::chrono::duration<float, std::milli>(cmdResetEnd - cmdResetStart).count();

            const auto cmdRecordStart = std::chrono::steady_clock::now();
            recordCommandBuffer(renderList, uiBuffer, frame.commandBuffer, imageIndex);
            const auto cmdRecordEnd = std::chrono::steady_clock::now();
            frameStats.backendCmdRecordCpuMs =
                std::chrono::duration<float, std::milli>(cmdRecordEnd - cmdRecordStart).count();

            if (config.headless && diagnosticFramesLogged < 5)
            {
                logHeadlessFrameDiagnostics(renderList);
                diagnosticFramesLogged += 1;
            }

            // Submit command buffer
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            if (beginResult.waitSemaphore != VK_NULL_HANDLE)
            {
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = &beginResult.waitSemaphore;
                submitInfo.pWaitDstStageMask = waitStages;
            }

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &frame.commandBuffer;

            VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
            if (frameOutputTarget->requiresRenderFinishedSemaphore())
            {
                renderFinishedSemaphore = frameManager->getRenderFinishedSemaphore(imageIndex);
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
            }

            const auto submitStart = std::chrono::steady_clock::now();
            if (vkQueueSubmit(vcsheet::getGraphicsQueue(), 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }
            const auto submitEnd = std::chrono::steady_clock::now();
            frameStats.backendQueueSubmitCpuMs =
                std::chrono::duration<float, std::milli>(submitEnd - submitStart).count();

            if (frameManager->getImagesInFlightCount() > imageIndex)
            {
                frameManager->setImageFence(imageIndex, frame.inFlightFence);
            }

            if (tryConsumeScreenshotRequest(dt))
            {
                vkWaitForFences(vcsheet::getDevice(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
                if (config.headless)
                    logHeadlessFrameDiagnostics(renderList);
                screenshotManager.saveImage(
                    frameOutputTarget->getImages()[imageIndex],
                    frameOutputTarget->getFormat(),
                    frameOutputTarget->getExtent(),
                    frameOutputTarget->getUiFinalLayout());
                screenshotRequested = false;
                ++screenshotsTaken;
                secondsSinceLastScreenshot = 0.0f;
            }

            // fire the on frame ended event, currently non functional
            if (lastFrameTime >= frameInterval)
            {
                eventBus.emit(GtsFrameEndedEvent{dt, imageIndex});
                lastFrameTime -= frameInterval;
                frameCounter++;
            }

            const auto presentStart = std::chrono::steady_clock::now();
            frameOutputTarget->endFrame(imageIndex, renderFinishedSemaphore);
            const auto presentEnd = std::chrono::steady_clock::now();
            frameStats.backendPresentCpuMs =
                std::chrono::duration<float, std::milli>(presentEnd - presentStart).count();

            currentFrame = (currentFrame + 1) % GraphicsConstants::MAX_FRAMES_IN_FLIGHT;
            const auto frameEnd = std::chrono::steady_clock::now();
            frameStats.backendFrameCpuMs =
                std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
        }

        // filler getters, not to be used in the real program
        Attachment* getAttachmentWrapper() { return depthAttachment.get(); }
        RenderResourceManager* getResourceSystem() { return resourceSystem.get(); }
};
