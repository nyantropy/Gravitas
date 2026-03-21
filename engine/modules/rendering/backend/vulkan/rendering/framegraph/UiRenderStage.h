#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "GtsFrameGraph.h"
#include "GtsRenderStage.h"
#include "VulkanUiRenderer.hpp"
#include "RenderResourceManager.hpp"
#include <UICommand.h>

// UI overlay render stage.
// Delegates to VulkanUiRenderer::record(), compositing text over the 3D scene.
// Reads the swapchain in PRESENT_SRC_KHR (left by SceneRenderStage) and
// writes it back to PRESENT_SRC_KHR — same layout, so no barrier is inserted.
class UiRenderStage : public GtsRenderStage
{
public:
    UiRenderStage(VulkanUiRenderer*      uiRenderer,
                  RenderResourceManager* resources,
                  GtsResourceHandle      swapchainHandle)
        : GtsRenderStage("UiRenderStage")
        , uiRenderer(uiRenderer)
        , resources(resources)
        , swapchainHandle(swapchainHandle)
    {}

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<std::vector<UICommandList>>(this);

        // Read dependency on the swapchain — creates the scene→UI ordering edge.
        graph.declareRead(this, swapchainHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // Write back to PRESENT_SRC_KHR (same layout, no barrier needed).
        graph.declareWrite(this, swapchainHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                uint32_t imageIndex, uint32_t currentFrame) override
    {
        const auto& uiLists = graph.getData<std::vector<UICommandList>>();
        uiRenderer->record(cmd, imageIndex, currentFrame, uiLists, resources);
    }

private:
    VulkanUiRenderer*      uiRenderer;
    RenderResourceManager* resources;
    GtsResourceHandle      swapchainHandle;
};
