#pragma once

#include <memory>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "GlmConfig.h"

#include "GtsFrameGraph.h"
#include "GtsRenderStage.h"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"
#include "VulkanPipeline.hpp"
#include "VulkanPipelineConfig.h"
#include "VulkanFramebufferSet.hpp"
#include "VulkanFramebufferSetConfig.h"
#include "RenderResourceManager.hpp"
#include "GraphicsConstants.h"
#include "UIGlyphVertexDescription.h"
#include "dssheet.h"
#include "vcsheet.h"
#include "VulkanDynamicBuffer.h"
#include <UICommand.h>
#include "GtsFrameStats.h"
#include "GtsDebugOverlay.h"

// Text render stage.
// Renders both screen-space (UITextComponent) and world-space (QuadTextComponent)
// text in a single pass.  World-space quads arrive pre-transformed in NDC [0..1]
// via the UICommandList blackboard — transformation is done by WorldTextCommandExtractor
// before renderFrame is called.
//
// Owns its render pass, pipeline, framebuffers, and dynamic vertex/index buffers.
// Composites text over the 3D scene by reading the swapchain in PRESENT_SRC_KHR
// (left by SceneRenderStage) and writing it back to the same layout.
class TextRenderStage : public GtsRenderStage
{
public:
    TextRenderStage(RenderResourceManager* resources,
                    GtsResourceHandle      swapchainHandle)
        : GtsRenderStage("TextRenderStage")
        , resources(resources)
        , swapchainHandle(swapchainHandle)
    {
        // Render pass — LOAD_OP_LOAD, no depth, composites onto the 3D scene.
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat        = vcsheet::getSwapChainImageFormat();
        rpConfig.colorLoadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        rpConfig.colorInitialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        rpConfig.hasDepthAttachment = false;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        // Pipeline — alpha blend, no depth, UI vertex layout, push constant mat4.
        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath   = GraphicsConstants::UI_V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::UI_F_SHADER_PATH;
        pConfig.vkRenderPass       = renderPass->getRenderPass();
        {
            auto b = UIGlyphVertexDescription::getBindingDescription();
            auto a = UIGlyphVertexDescription::getAttributeDescriptions();
            pConfig.vertexBinding    = b;
            pConfig.vertexAttributes = std::vector<VkVertexInputAttributeDescription>(a.begin(), a.end());
        }
        pConfig.depthTestEnable      = false;
        pConfig.depthWriteEnable     = false;
        pConfig.blendEnable          = true;
        pConfig.srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
        pConfig.dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pConfig.colorBlendOp         = VK_BLEND_OP_ADD;
        pConfig.srcAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
        pConfig.dstAlphaBlendFactor  = VK_BLEND_FACTOR_ZERO;
        pConfig.alphaBlendOp         = VK_BLEND_OP_ADD;
        pConfig.pushConstantSize     = sizeof(glm::mat4);
        pConfig.pushConstantStages   = VK_SHADER_STAGE_VERTEX_BIT;
        pConfig.descriptorSetLayouts = { dssheet::getDescriptorSetLayouts()[2] };
        pipeline = std::make_unique<VulkanPipeline>(pConfig);

        // Framebuffers — swapchain image views only, no depth.
        const auto& imageViews = vcsheet::getSwapChainImageViews();
        const auto  extent     = vcsheet::getSwapChainExtent();

        VulkanFramebufferSetConfig fbConfig;
        fbConfig.renderPass = renderPass->getRenderPass();
        fbConfig.width      = extent.width;
        fbConfig.height     = extent.height;
        fbConfig.layers     = 1;
        fbConfig.attachmentsPerFramebuffer.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); ++i)
            fbConfig.attachmentsPerFramebuffer[i] = { imageViews[i] };
        framebuffers = std::make_unique<VulkanFramebufferSet>(fbConfig);

        debugOverlay.init(resources);
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<std::vector<UICommandList>>(this);
        graph.requestData<GtsFrameStats>(this);

        // Read dependency on the swapchain — creates the scene→text ordering edge.
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
        // Get ECS UI batch (screen-space + pre-transformed world-space) and frame stats.
        const auto& uiListsFromBlackboard = graph.getData<std::vector<UICommandList>>();
        const auto& stats                 = graph.getData<GtsFrameStats>();

        // Build the full batch (game UI + optional debug overlay) as a mutable copy.
        std::vector<UICommandList> uiLists = uiListsFromBlackboard;

        if (debugOverlay.isEnabled())
            debugOverlay.appendToBatch(uiLists, stats);

        if (uiLists.empty())
            return;

        // Upload all vertices/indices (all batches concatenated).
        uint32_t totalVerts   = 0;
        uint32_t totalIndices = 0;
        for (const auto& list : uiLists)
        {
            totalVerts   += static_cast<uint32_t>(list.vertices.size());
            totalIndices += static_cast<uint32_t>(list.indices.size());
        }

        if (totalVerts == 0)
            return;

        vertexBuffer.ensureCapacity(totalVerts   * sizeof(UIGlyphVertex));
        indexBuffer.ensureCapacity(totalIndices  * sizeof(uint32_t));

        auto* vDst = static_cast<UIGlyphVertex*>(vertexBuffer.getMapped());
        auto* iDst = static_cast<uint32_t*>(indexBuffer.getMapped());
        uint32_t vertOffset = 0;
        uint32_t idxOffset  = 0;
        for (const auto& list : uiLists)
        {
            std::memcpy(vDst + vertOffset, list.vertices.data(),
                        list.vertices.size() * sizeof(UIGlyphVertex));

            // Indices must be rebased to the concatenated vertex offset.
            for (uint32_t idx : list.indices)
                iDst[idxOffset++] = idx + vertOffset;

            vertOffset += static_cast<uint32_t>(list.vertices.size());
        }

        // Begin text render pass.
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = renderPass->getRenderPass();
        rpInfo.framebuffer       = framebuffers->getFramebuffers()[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = vcsheet::getSwapChainExtent();
        rpInfo.clearValueCount   = 0;
        rpInfo.pClearValues      = nullptr;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.x        = 0.0f;
        vp.y        = 0.0f;
        vp.width    = static_cast<float>(vcsheet::getSwapChainExtent().width);
        vp.height   = static_cast<float>(vcsheet::getSwapChainExtent().height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vcsheet::getSwapChainExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

        // DONT EVER CHANGE THIS, WE HAVE TO FLIP TOP AND BOT BECAUSE OF GLM SETTINGS, SO DONT CHANGE THIS
        glm::mat4 proj = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
        vkCmdPushConstants(cmd, pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &proj);

        const VkDeviceSize offsets[] = {0};
        VkBuffer vb = vertexBuffer.getBuffer();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
        VkBuffer ib = indexBuffer.getBuffer();
        vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);

        // One draw call per atlas.
        uint32_t indexBase = 0;
        for (const auto& list : uiLists)
        {
            TextureResource* tex = resources->getTexture(list.textureID);
            if (!tex) { indexBase += static_cast<uint32_t>(list.indices.size()); continue; }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(), 0, 1,
                                    &tex->descriptorSets[currentFrame],
                                    0, nullptr);

            vkCmdDrawIndexed(cmd,
                             static_cast<uint32_t>(list.indices.size()),
                             1, indexBase, 0, 0);

            indexBase += static_cast<uint32_t>(list.indices.size());
        }

        vkCmdEndRenderPass(cmd);
    }

    GtsDebugOverlay& getDebugOverlay() { return debugOverlay; }

private:
    std::unique_ptr<VulkanRenderPass>     renderPass;
    std::unique_ptr<VulkanPipeline>       pipeline;
    std::unique_ptr<VulkanFramebufferSet> framebuffers;
    RenderResourceManager*                resources;
    GtsResourceHandle                     swapchainHandle;

    GtsDebugOverlay                       debugOverlay;

    // Dynamic buffers — host-visible, persistently mapped, resized on demand.
    static constexpr uint32_t INITIAL_VERTEX_CAP = 4096;
    static constexpr uint32_t INITIAL_INDEX_CAP  = 8192;

    VulkanDynamicBuffer vertexBuffer{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                      INITIAL_VERTEX_CAP * sizeof(UIGlyphVertex)};
    VulkanDynamicBuffer indexBuffer {VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                      INITIAL_INDEX_CAP  * sizeof(uint32_t)};
};
