#pragma once

#include <vulkan/vulkan.h>

#include "Attachment.hpp"
#include "GtsFrameGraph.h"
#include "GtsFrameStats.h"
#include "RenderResourceManager.hpp"
#include "SceneRenderStage.h"
#include "ParticleRenderStage.h"
#include "UpscaleRenderStage.h"
#include "UiRenderStage.h"
#include "VulkanBackendContext.h"
#include "output/IFrameOutputTarget.hpp"
#include "threading/ThreadPool.h"

struct ForwardFrameGraphBuildConfig
{
    RenderResourceManager& resources;
    ThreadPool& threadPool;
    VulkanBackendContext& backendContext;
    IFrameOutputTarget& frameOutputTarget;
    Attachment& depthAttachment;
    Attachment* sceneColorAttachment;
    VkFormat depthFormat;
    VkExtent2D sceneRenderExtent;
    GtsFrameStats& frameStats;
    bool useInternalScaling;
    bool debugOverlayEnabled;
};

struct ForwardFrameGraphStageRefs
{
    SceneRenderStage* sceneStage = nullptr;
    ParticleRenderStage* particleStage = nullptr;
    UiRenderStage* uiStage = nullptr;
};

class ForwardFrameGraphBuilder
{
public:
    static ForwardFrameGraphStageRefs build(GtsFrameGraph& graph,
                                            const ForwardFrameGraphBuildConfig& config);
};
