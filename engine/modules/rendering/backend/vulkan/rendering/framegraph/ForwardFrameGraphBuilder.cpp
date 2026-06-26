#include "ForwardFrameGraphBuilder.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

ForwardFrameGraphStageRefs ForwardFrameGraphBuilder::build(
    GtsFrameGraph& graph,
    const ForwardFrameGraphBuildConfig& config)
{
    const auto& images = config.frameOutputTarget.getImages();
    const auto& imageViews = config.frameOutputTarget.getImageViews();
    const auto format = config.frameOutputTarget.getFormat();
    const auto outputExtent = config.frameOutputTarget.getExtent();

    GtsResourceHandle outputHandle0 = GTS_INVALID_RESOURCE;
    for (uint32_t i = 0; i < static_cast<uint32_t>(images.size()); ++i)
    {
        GtsResourceHandle handle = graph.importResource(
            "frame_output_" + std::to_string(i),
            images[i],
            imageViews[i],
            format,
            outputExtent,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED);
        graph.registerSwapchainHandle(i, handle);
        if (i == 0)
            outputHandle0 = handle;
    }

    GtsResourceHandle sceneColorHandle = outputHandle0;
    std::vector<VkImageView> sceneColorViews(imageViews.begin(), imageViews.end());
    VkImageLayout sceneColorInitialLayout = config.frameOutputTarget.getUiInitialLayout();
    VkImageLayout sceneColorFinalLayout = config.frameOutputTarget.getUiInitialLayout();
    DescriptorSetManager& descriptorSetManager = config.resources.getDescriptorSetManager();

    if (config.useInternalScaling)
    {
        assert(config.sceneColorAttachment != nullptr && "Internal scaling requires a scene color attachment");
        sceneColorHandle = graph.importResource(
            "scene_color",
            config.sceneColorAttachment->getImage(),
            config.sceneColorAttachment->getImageView(),
            format,
            config.sceneRenderExtent,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED);
        sceneColorViews = {config.sceneColorAttachment->getImageView()};
        sceneColorInitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        sceneColorFinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    GtsResourceHandle depthHandle = graph.importResource(
        "depth",
        config.depthAttachment.getImage(),
        config.depthAttachment.getImageView(),
        config.depthFormat,
        config.sceneRenderExtent,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED);

    ForwardFrameGraphStageRefs refs;

    auto ownedScene = std::make_unique<SceneRenderStage>(
        &config.resources,
        &config.threadPool,
        config.backendContext,
        descriptorSetManager,
        sceneColorViews,
        static_cast<uint32_t>(images.size()),
        format,
        config.sceneRenderExtent,
        config.depthAttachment.getImageView(),
        config.depthFormat,
        sceneColorHandle,
        depthHandle,
        sceneColorFinalLayout);
    refs.sceneStage = ownedScene.get();
    graph.addStage(std::move(ownedScene));

    auto ownedParticles = std::make_unique<ParticleRenderStage>(
        &config.resources,
        config.backendContext,
        descriptorSetManager,
        sceneColorViews,
        static_cast<uint32_t>(images.size()),
        format,
        config.sceneRenderExtent,
        config.depthAttachment.getImageView(),
        config.depthFormat,
        sceneColorHandle,
        depthHandle,
        sceneColorInitialLayout,
        sceneColorFinalLayout);
    refs.particleStage = ownedParticles.get();
    graph.addStage(std::move(ownedParticles));

    if (config.useInternalScaling)
    {
        auto ownedUpscale = std::make_unique<UpscaleRenderStage>(
            config.backendContext,
            descriptorSetManager,
            config.sceneColorAttachment->getImageView(),
            config.sceneRenderExtent,
            format,
            sceneColorHandle,
            outputHandle0,
            config.frameOutputTarget.getUiInitialLayout());
        graph.addStage(std::move(ownedUpscale));
    }

    auto ownedUi = std::make_unique<UiRenderStage>(
        &config.resources,
        config.backendContext,
        descriptorSetManager,
        outputHandle0,
        &config.frameStats,
        config.debugOverlayEnabled,
        config.frameOutputTarget.getUiInitialLayout(),
        config.frameOutputTarget.getUiFinalLayout());
    refs.uiStage = ownedUi.get();
    graph.addStage(std::move(ownedUi));

    graph.compile();
    return refs;
}
