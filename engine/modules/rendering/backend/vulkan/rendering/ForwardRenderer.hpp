#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "Renderer.hpp"
#include "RendererConfig.h"

#include "VulkanBackendContext.h"
#include "Attachment.hpp"
#include "AttachmentConfig.h"

#include "RenderResourceManager.hpp"
#include "RenderCommandExtractor.hpp"

#include "GraphicsConstants.h"
#include "FormatUtil.hpp"

#include "FrameManager.hpp"
#include "ScreenshotManager.hpp"
#include "threading/ThreadPool.h"

#include "GtsPlatformEventBus.hpp"
#include "GtsEventTypes.h"

#include <UiCommand.h>
#include "GtsFrameStats.h"

#include "GtsFrameGraph.h"
#include "ForwardFrameGraphBuilder.h"
#include "output/HeadlessFrameOutputTarget.hpp"
#include "output/WindowedFrameOutputTarget.hpp"

class ForwardRenderer : Renderer
{
    private:
        // Frame-level resources shared across all stages.
        std::unique_ptr<Attachment>            sceneColorAttachment;
        std::unique_ptr<Attachment>            depthAttachment;
        std::unique_ptr<Attachment>            editorPreviewColorAttachment;
        std::unique_ptr<Attachment>            editorPreviewDepthAttachment;
        std::unique_ptr<IFrameOutputTarget>    frameOutputTarget;
        std::unique_ptr<RenderResourceManager> resourceSystem;
        std::unique_ptr<FrameManager>          frameManager;
        std::unique_ptr<ThreadPool>            threadPool;
        VulkanBackendContext&                  backendContext;

        // Per-frame stats — populated in renderFrame, provided to the blackboard.
        GtsFrameStats frameStats;

        // Raw pointers into the frame graph — set during buildFrameGraph().
        SceneRenderStage* sceneStage = nullptr;
        ParticleRenderStage* particleStage = nullptr;
        SceneRenderStage* editorPreviewSceneStage = nullptr;
        ParticleRenderStage* editorPreviewParticleStage = nullptr;
        UiRenderStage*    uiStage    = nullptr;

        // Depth format — resolved once in createDepthAttachment(), reused by
        // buildFrameGraph() and SceneRenderStage.
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D sceneRenderExtent{1, 1};

        // Frame graph — built once in createResources(), executed every frame.
        // Declared after frame resources so stages are destroyed before attachments.
        GtsFrameGraph frameGraph;

        // misc variables we need for the draw loop
        float FPS = 30.0f;
        double frameInterval = 1.0 / FPS;
        double lastFrameTime = 0.0;
        int frameCounter = 0;

        uint32_t currentFrame = 0;
        bool framebufferResized = false;
        bool frameOutputRecreateRequested = false;
        bool debugOverlayEnabled = false;
        bool screenshotRequested = false;
        std::string screenshotOutputDirectory;
        uint32_t screenshotsTaken = 0;
        uint32_t renderedFrameCount = 0;
        uint32_t diagnosticFramesLogged = 0;
        float secondsSinceLastScreenshot = 1000.0f;
        bool maxScreenshotWarningLogged = false;
        bool minIntervalWarningLogged = false;
        ScreenshotManager screenshotManager;
        GtsPlatformEventBus& eventBus;
        VkSampler editorPreviewSampler = VK_NULL_HANDLE;
        texture_id_type editorPreviewTextureID = 0;
        VkExtent2D editorPreviewExtent{0, 0};


        VkFormat findDepthFormat()
        {
            std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            return FormatUtil::findSupportedFormat(backendContext.physicalDevice(), candidates, tiling, features);
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
                backendContext.physicalDevice(),
                candidates,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
        }

        bool useInternalScaling() const
        {
            return !config.headless && config.internalScalingEnabled;
        }

        static bool viewportEquals(const RenderViewportRect& lhs, const RenderViewportRect& rhs)
        {
            return lhs.x == rhs.x
                && lhs.y == rhs.y
                && lhs.width == rhs.width
                && lhs.height == rhs.height;
        }

        static RenderViewportRect scaleViewportRect(const RenderViewportRect& viewport,
                                                    VkExtent2D                 sourceExtent,
                                                    VkExtent2D                 targetExtent)
        {
            const float sourceWidth  = static_cast<float>(std::max(1u, sourceExtent.width));
            const float sourceHeight = static_cast<float>(std::max(1u, sourceExtent.height));
            const float targetWidth  = static_cast<float>(std::max(1u, targetExtent.width));
            const float targetHeight = static_cast<float>(std::max(1u, targetExtent.height));

            RenderViewportRect scaled{
                static_cast<int>(std::round(static_cast<float>(viewport.x) * targetWidth / sourceWidth)),
                static_cast<int>(std::round(static_cast<float>(viewport.y) * targetHeight / sourceHeight)),
                static_cast<int>(std::round(static_cast<float>(viewport.width) * targetWidth / sourceWidth)),
                static_cast<int>(std::round(static_cast<float>(viewport.height) * targetHeight / sourceHeight))};
            return scaled.clampedTo(static_cast<int>(targetExtent.width),
                                    static_cast<int>(targetExtent.height));
        }

        RenderViewportFrame buildViewportFrame(const RenderViewportRect& requestedViewport) const
        {
            const VkExtent2D outputExtent = frameOutputTarget->getExtent();
            const RenderViewportRect fullOutput =
                RenderViewportRect::full(static_cast<int>(outputExtent.width), static_cast<int>(outputExtent.height));
            const RenderViewportRect outputViewport =
                requestedViewport.valid()
                    ? requestedViewport.clampedTo(static_cast<int>(outputExtent.width),
                                                  static_cast<int>(outputExtent.height))
                    : fullOutput;

            RenderViewportFrame frame;
            frame.outputViewport = outputViewport;
            frame.constrained    = !viewportEquals(outputViewport, fullOutput);

            if (useInternalScaling())
            {
                frame.sceneRenderViewport = scaleViewportRect(outputViewport, outputExtent, sceneRenderExtent);
            }
            else
            {
                frame.sceneRenderViewport =
                    outputViewport.clampedTo(static_cast<int>(sceneRenderExtent.width),
                                             static_cast<int>(sceneRenderExtent.height));
            }

            return frame;
        }

        void createFrameOutputTarget()
        {
            if (config.headless)
            {
                const VkFormat colorFormat = findHeadlessColorFormat();
                frameOutputTarget = std::make_unique<HeadlessFrameOutputTarget>(
                    backendContext,
                    config.renderWidth,
                    config.renderHeight,
                    colorFormat);
            }
            else
            {
                frameOutputTarget = std::make_unique<WindowedFrameOutputTarget>(backendContext);
            }

            backendContext.vulkanContext().registerFrameOutput(
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
            attachConfig.width = sceneRenderExtent.width;
            attachConfig.height = sceneRenderExtent.height;
            attachConfig.format = depthFormat;
            attachConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            attachConfig.imageUsageFlags =
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            attachConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            attachConfig.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthAttachment = std::make_unique<Attachment>(backendContext, attachConfig);
        }

        void createSceneTargets()
        {
            sceneRenderExtent = useInternalScaling()
                ? VkExtent2D{std::max(1u, config.renderWidth), std::max(1u, config.renderHeight)}
                : frameOutputTarget->getExtent();

            if (!useInternalScaling())
                return;

            AttachmentConfig attachConfig;
            attachConfig.width = sceneRenderExtent.width;
            attachConfig.height = sceneRenderExtent.height;
            attachConfig.format = frameOutputTarget->getFormat();
            attachConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            attachConfig.imageUsageFlags =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            attachConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            attachConfig.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            sceneColorAttachment = std::make_unique<Attachment>(backendContext, attachConfig);
        }

        void buildFrameGraph()
        {
            const ForwardFrameGraphBuildConfig buildConfig{
                *resourceSystem,
                *threadPool,
                backendContext,
                *frameOutputTarget,
                *depthAttachment,
                sceneColorAttachment.get(),
                editorPreviewColorAttachment.get(),
                editorPreviewDepthAttachment.get(),
                depthFormat,
                sceneRenderExtent,
                frameStats,
                useInternalScaling(),
                debugOverlayEnabled};

            const ForwardFrameGraphStageRefs stageRefs =
                ForwardFrameGraphBuilder::build(frameGraph, buildConfig);
            sceneStage = stageRefs.sceneStage;
            particleStage = stageRefs.particleStage;
            editorPreviewSceneStage = stageRefs.editorPreviewSceneStage;
            editorPreviewParticleStage = stageRefs.editorPreviewParticleStage;
            uiStage = stageRefs.uiStage;
        }

        void createFrameResources()
        {
            createFrameOutputTarget();
            createSceneTargets();
            createDepthAttachment();
            frameManager   = std::make_unique<FrameManager>(backendContext,
                                                            frameOutputTarget->getImages().size(),
                                                            frameOutputTarget->requiresRenderFinishedSemaphore());
            threadPool     = std::make_unique<ThreadPool>();
            buildFrameGraph();
        }

        void createResources() override
        {
            resourceSystem = std::make_unique<RenderResourceManager>(backendContext);
            createFrameResources();
        }

        void destroyFrameResources()
        {
            frameGraph.clear();
            sceneStage = nullptr;
            particleStage = nullptr;
            editorPreviewSceneStage = nullptr;
            editorPreviewParticleStage = nullptr;
            uiStage = nullptr;
            if (threadPool) threadPool.reset();
            if (frameManager) frameManager.reset();
            if (depthAttachment) depthAttachment.reset();
            if (sceneColorAttachment) sceneColorAttachment.reset();
            if (frameOutputTarget) frameOutputTarget.reset();
            currentFrame = 0;
            framebufferResized = false;
        }

        void rebuildFrameGraph()
        {
            frameGraph.clear();
            sceneStage = nullptr;
            particleStage = nullptr;
            editorPreviewSceneStage = nullptr;
            editorPreviewParticleStage = nullptr;
            uiStage = nullptr;
            buildFrameGraph();
        }

        void createEditorPreviewSampler()
        {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;

            if (vkCreateSampler(backendContext.device(), &samplerInfo, nullptr, &editorPreviewSampler) != VK_SUCCESS)
                throw std::runtime_error("failed to create editor preview sampler");
        }

        void destroyEditorPreviewTarget()
        {
            if (resourceSystem && editorPreviewTextureID != 0)
                resourceSystem->unregisterTexture(editorPreviewTextureID);
            editorPreviewTextureID = 0;

            if (editorPreviewSampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(backendContext.device(), editorPreviewSampler, nullptr);
                editorPreviewSampler = VK_NULL_HANDLE;
            }

            editorPreviewDepthAttachment.reset();
            editorPreviewColorAttachment.reset();
            editorPreviewExtent = {0, 0};
        }

        void createEditorPreviewTarget(uint32_t width, uint32_t height)
        {
            editorPreviewExtent = {std::max(1u, width), std::max(1u, height)};

            AttachmentConfig colorConfig;
            colorConfig.width = editorPreviewExtent.width;
            colorConfig.height = editorPreviewExtent.height;
            colorConfig.format = frameOutputTarget->getFormat();
            colorConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            colorConfig.imageUsageFlags =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            colorConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            colorConfig.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            editorPreviewColorAttachment = std::make_unique<Attachment>(backendContext, colorConfig);

            AttachmentConfig depthConfig;
            depthConfig.width = editorPreviewExtent.width;
            depthConfig.height = editorPreviewExtent.height;
            depthConfig.format = depthFormat;
            depthConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
            depthConfig.imageUsageFlags =
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            depthConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            depthConfig.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            editorPreviewDepthAttachment = std::make_unique<Attachment>(backendContext, depthConfig);

            createEditorPreviewSampler();
            editorPreviewTextureID = resourceSystem->registerSampledImageTexture(
                "editor_preview_color_" + std::to_string(editorPreviewExtent.width) + "x" +
                    std::to_string(editorPreviewExtent.height),
                editorPreviewColorAttachment->getImageView(),
                editorPreviewSampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                static_cast<int>(editorPreviewExtent.width),
                static_cast<int>(editorPreviewExtent.height));
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
                minIntervalWarningLogged = false;
                screenshotRequested = false;
                screenshotOutputDirectory.clear();
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
                screenshotOutputDirectory.clear();
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
                << " pipelineBinds=" << frameStats.pipelineBinds
                << " descriptorBinds=" << frameStats.descriptorBinds
                << " triangles=" << frameStats.triangleCount
                << std::endl;
        }

        void recordCommandBuffer(const std::vector<RenderCommand>& renderList,
                                 const MaterialFrameData& materialFrameData,
                                 const ParticleFrameData& particleData,
                                 const RenderViewportRect& sceneViewport,
                                 const UiCommandBuffer& uiBuffer,
                                 const EditorPreviewRenderData& editorPreview,
                                 VkCommandBuffer commandBuffer, uint32_t imageIndex)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            const RenderViewportFrame viewportFrame = buildViewportFrame(sceneViewport);
            frameGraph.provideData(&renderList);
            frameGraph.provideData(&materialFrameData);
            frameGraph.provideData(&particleData);
            frameGraph.provideData(&viewportFrame);
            frameGraph.provideData(&uiBuffer);
            frameGraph.provideData(&editorPreview);
            frameGraph.provideData(&frameStats);
            frameGraph.execute(commandBuffer, imageIndex, currentFrame);

            // Render stats are written by SceneRenderStage during execute — read them back.
            if (sceneStage)
            {
                frameStats.triangleCount    = sceneStage->getLastTriangleCount();
                frameStats.drawCalls        = sceneStage->getLastDrawCalls();
                frameStats.pipelineBinds    = sceneStage->getLastPipelineBinds();
                frameStats.descriptorBinds  = sceneStage->getLastDescriptorBinds();
                frameStats.pipelineSwitches = sceneStage->getLastPipelineSwitches();
                frameStats.textureSwitches  = sceneStage->getLastTextureSwitches();
            }

            if (particleStage)
            {
                frameStats.particleCount     = particleStage->getLastParticleCount();
                frameStats.particleDrawCalls = particleStage->getLastDrawCalls();
                frameStats.drawCalls        += frameStats.particleDrawCalls;
            }

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        }


    public:
        ForwardRenderer(RendererConfig config,
                        VulkanBackendContext& backendContext,
                        GtsPlatformEventBus& eventBus)
            : Renderer(config)
            , backendContext(backendContext)
            , frameGraph(backendContext)
            , screenshotManager(backendContext)
            , eventBus(eventBus)
        {
            createResources();
        }

        ~ForwardRenderer()
        {
            releaseEditorPreviewTarget();
            destroyFrameResources();
            if (resourceSystem)  resourceSystem.reset();
        }

        void toggleDebugOverlay() override
        {
            debugOverlayEnabled = !debugOverlayEnabled;
            if (uiStage)
                uiStage->getDebugOverlay().setEnabled(debugOverlayEnabled);
        }

        void cycleDebugOverlayPage()
        {
            if (uiStage)
                uiStage->getDebugOverlay().nextPage();
        }

        void requestScreenshot(const std::string& outputDirectory = {}) override
        {
            screenshotRequested = true;
            screenshotOutputDirectory = outputDirectory;
        }

        void recreateFrameResources()
        {
            destroyFrameResources();
            createFrameResources();
            frameOutputRecreateRequested = false;
        }

        void setRenderResolution(uint32_t width, uint32_t height)
        {
            config.renderWidth = std::max(1u, width);
            config.renderHeight = std::max(1u, height);
        }

        void setInternalScalingEnabled(bool enabled)
        {
            config.internalScalingEnabled = enabled;
        }

        void releaseFrameResources()
        {
            destroyFrameResources();
        }

        void rebuildFrameResources()
        {
            createFrameResources();
            frameOutputRecreateRequested = false;
        }

        texture_id_type ensureEditorPreviewTarget(uint32_t width, uint32_t height) override
        {
            width = std::max(1u, width);
            height = std::max(1u, height);
            if (editorPreviewTextureID != 0 &&
                editorPreviewExtent.width == width &&
                editorPreviewExtent.height == height)
            {
                return editorPreviewTextureID;
            }

            vkDeviceWaitIdle(backendContext.device());
            frameGraph.clear();
            sceneStage = nullptr;
            particleStage = nullptr;
            editorPreviewSceneStage = nullptr;
            editorPreviewParticleStage = nullptr;
            uiStage = nullptr;
            destroyEditorPreviewTarget();
            createEditorPreviewTarget(width, height);
            buildFrameGraph();
            return editorPreviewTextureID;
        }

        void releaseEditorPreviewTarget() override
        {
            if (editorPreviewTextureID == 0 &&
                editorPreviewColorAttachment == nullptr &&
                editorPreviewDepthAttachment == nullptr &&
                editorPreviewSampler == VK_NULL_HANDLE)
            {
                return;
            }

            vkDeviceWaitIdle(backendContext.device());
            frameGraph.clear();
            sceneStage = nullptr;
            particleStage = nullptr;
            editorPreviewSceneStage = nullptr;
            editorPreviewParticleStage = nullptr;
            uiStage = nullptr;
            destroyEditorPreviewTarget();
            if (frameOutputTarget && depthAttachment && resourceSystem && threadPool)
                buildFrameGraph();
        }

        bool consumeFrameOutputRecreateRequested()
        {
            const bool requested = frameOutputRecreateRequested;
            frameOutputRecreateRequested = false;
            return requested;
        }

        void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                         const MaterialFrameData& materialFrameData,
                         const std::vector<ObjectUploadCommand>& objectUploads,
                         const std::vector<CameraUploadCommand>& cameraUploads,
                         const ParticleFrameData& particleData,
                         const RenderViewportRect& sceneViewport,
                         const UiCommandBuffer& uiBuffer,
                         const EditorPreviewRenderData& editorPreview,
                         const GtsFrameStats& stats) override
        {
            const auto frameStart = std::chrono::steady_clock::now();
            renderedFrameCount += 1;

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
            vkWaitForFences(backendContext.device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
            const auto fenceWaitEnd = std::chrono::steady_clock::now();
            frameStats.backendFenceWaitCpuMs =
                std::chrono::duration<float, std::milli>(fenceWaitEnd - fenceWaitStart).count();

            for (const auto& upload : cameraUploads)
            {
                resourceSystem->uploadCameraViewFrame(
                    currentFrame,
                    upload.cameraViewID,
                    upload.viewMatrix,
                    upload.projMatrix,
                    upload.cameraWorldPosition,
                    upload.directionalLight);
            }
            if (editorPreview.enabled)
            {
                for (const auto& upload : editorPreview.cameraUploads)
                {
                    resourceSystem->uploadCameraViewFrame(
                        currentFrame,
                        upload.cameraViewID,
                        upload.viewMatrix,
                        upload.projMatrix,
                        upload.cameraWorldPosition,
                        upload.directionalLight);
                }
            }

            // Acquire frame output image
            const auto acquireStart = std::chrono::steady_clock::now();
            FrameOutputBeginResult beginResult =
                frameOutputTarget->beginFrame(currentFrame, frame.imageAvailableSemaphore);
            const auto acquireEnd = std::chrono::steady_clock::now();
            frameStats.backendAcquireCpuMs =
                std::chrono::duration<float, std::milli>(acquireEnd - acquireStart).count();

            if (beginResult.imageIndex == UINT32_MAX)
            {
                frameOutputRecreateRequested = true;
                return;
            }

            const uint32_t imageIndex = beginResult.imageIndex;

            VkFence imageFence = frameManager->getImageFence(imageIndex);
            if (frameManager->getImagesInFlightCount() > imageIndex && imageFence != VK_NULL_HANDLE)
            {
                const auto imageWaitStart = std::chrono::steady_clock::now();
                vkWaitForFences(backendContext.device(), 1, &imageFence, VK_TRUE, UINT64_MAX);
                const auto imageWaitEnd = std::chrono::steady_clock::now();
                frameStats.backendImageWaitCpuMs =
                    std::chrono::duration<float, std::milli>(imageWaitEnd - imageWaitStart).count();
            }

            // Object data is double/triple-buffered, so changed transforms or
            // per-object material state must be propagated to every in-flight
            // SSBO copy before we clear the dirty state.
            const auto objectWriteStart = std::chrono::steady_clock::now();
            frameStats.backendObjectWrites = 0;
            frameStats.backendObjectWritesSkipped = 0;
            for (const auto& upload : objectUploads)
            {
                const uint32_t writes =
                    resourceSystem->writeObjectDataAllFrames(
                        upload.objectSSBOSlot,
                        upload.modelMatrix,
                        upload.uvTransform);
                frameStats.backendObjectWrites += writes;
                if (writes == 0)
                    frameStats.backendObjectWritesSkipped += 1;
            }
            for (const RenderCommand& command : renderList)
            {
                const MaterialFrameState* material = materialFrameData.find(command.materialGpu);
                if (material == nullptr)
                    continue;

                const uint32_t writes =
                    resourceSystem->writeObjectTintAllFrames(command.objectSSBOSlot, material->baseColor);
                frameStats.backendObjectWrites += writes;
                if (writes == 0)
                    frameStats.backendObjectWritesSkipped += 1;
            }
            if (editorPreview.enabled)
            {
                for (const auto& upload : editorPreview.objectUploads)
                {
                    const uint32_t writes =
                        resourceSystem->writeObjectDataAllFrames(
                            upload.objectSSBOSlot,
                            upload.modelMatrix,
                            upload.uvTransform);
                    frameStats.backendObjectWrites += writes;
                    if (writes == 0)
                        frameStats.backendObjectWritesSkipped += 1;
                }
                for (const RenderCommand& command : editorPreview.renderList)
                {
                    const MaterialFrameState* material = editorPreview.materialFrameData.find(command.materialGpu);
                    if (material == nullptr)
                        continue;

                    const uint32_t writes =
                        resourceSystem->writeObjectTintAllFrames(command.objectSSBOSlot, material->baseColor);
                    frameStats.backendObjectWrites += writes;
                    if (writes == 0)
                        frameStats.backendObjectWritesSkipped += 1;
                }
            }
            resourceSystem->flushAllObjectSSBO();
            const auto objectWriteEnd = std::chrono::steady_clock::now();
            frameStats.backendObjectWriteCpuMs =
                std::chrono::duration<float, std::milli>(objectWriteEnd - objectWriteStart).count();

            // Reset & record command buffer
            const auto fenceResetStart = std::chrono::steady_clock::now();
            vkResetFences(backendContext.device(), 1, &frame.inFlightFence);
            const auto fenceResetEnd = std::chrono::steady_clock::now();
            frameStats.backendFenceResetCpuMs =
                std::chrono::duration<float, std::milli>(fenceResetEnd - fenceResetStart).count();

            const auto cmdResetStart = std::chrono::steady_clock::now();
            vkResetCommandBuffer(frame.commandBuffer, 0);
            const auto cmdResetEnd = std::chrono::steady_clock::now();
            frameStats.backendCmdResetCpuMs =
                std::chrono::duration<float, std::milli>(cmdResetEnd - cmdResetStart).count();

            const auto cmdRecordStart = std::chrono::steady_clock::now();
            recordCommandBuffer(renderList,
                                materialFrameData,
                                particleData,
                                sceneViewport,
                                uiBuffer,
                                editorPreview,
                                frame.commandBuffer,
                                imageIndex);
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
            if (vkQueueSubmit(backendContext.graphicsQueue(), 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
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
                vkWaitForFences(backendContext.device(), 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
                if (config.headless)
                    logHeadlessFrameDiagnostics(renderList);
                screenshotManager.saveImage(
                    frameOutputTarget->getImages()[imageIndex],
                    frameOutputTarget->getFormat(),
                    frameOutputTarget->getExtent(),
                    frameOutputTarget->getUiFinalLayout(),
                    screenshotOutputDirectory);
                screenshotRequested = false;
                screenshotOutputDirectory.clear();
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
            if (frameOutputTarget->endFrame(imageIndex, renderFinishedSemaphore))
                frameOutputRecreateRequested = true;
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
