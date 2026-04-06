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
#include "FramebufferManager.hpp"
#include "FramebufferManagerConfig.h"
#include "RenderResourceManager.hpp"
#include "GraphicsConstants.h"
#include "UiVertexDescription.h"
#include "dssheet.h"
#include "vcsheet.h"
#include "VulkanDynamicBuffer.h"
#include <UiCommand.h>
#include "GtsDebugOverlay.h"
#include "GtsFrameStats.h"

// UI primitive render stage.
// Renders textured quads (images, sprites, glyphs) and colored quads over the 3D scene.
// Receives a UiCommandBuffer from the blackboard — populated from the retained
// engine UI system. The debug overlay is appended here.
// directly from the GtsFrameStats pointer before upload.
//
// Composites over the scene by reading the swapchain in PRESENT_SRC_KHR
// and writing it back to the same layout.
class UiRenderStage : public GtsRenderStage
{
public:
    UiRenderStage(RenderResourceManager* resources,
                  GtsResourceHandle      outputHandle,
                  GtsFrameStats*         frameStats,
                  bool                   debugEnabledByDefault,
                  VkImageLayout          colorInitialLayout,
                  VkImageLayout          colorFinalLayout)
        : GtsRenderStage("UiRenderStage")
        , resources(resources)
        , outputHandle(outputHandle)
        , frameStats(frameStats)
        , colorInitialLayout(colorInitialLayout)
        , colorFinalLayout(colorFinalLayout)
    {
        // Render pass — LOAD_OP_LOAD, no depth, composites onto the text layer.
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat        = vcsheet::getFrameOutputFormat();
        rpConfig.colorLoadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        rpConfig.colorInitialLayout = colorInitialLayout;
        rpConfig.colorFinalLayout   = colorFinalLayout;
        rpConfig.hasDepthAttachment = false;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        // Pipeline — alpha blend, no depth, UI vertex layout, push constant mat4 + float.
        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath   = GraphicsConstants::UI_V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::UI_F_SHADER_PATH;
        pConfig.vkRenderPass       = renderPass->getRenderPass();
        {
            auto b = UiVertexDescription::getBindingDescription();
            auto a = UiVertexDescription::getAttributeDescriptions();
            pConfig.vertexBindings   = { b };
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
        pConfig.pushConstantSize     = sizeof(glm::mat4) + sizeof(float); // proj + useTexture
        pConfig.pushConstantStages   = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pConfig.descriptorSetLayouts = { dssheet::getDescriptorSetLayouts()[2] };
        pipeline = std::make_unique<VulkanPipeline>(pConfig);

        // Framebuffers — frame output image views only, no depth.
        FramebufferManagerConfig fbConfig;
        fbConfig.vkRenderpass       = renderPass->getRenderPass();
        fbConfig.hasDepthAttachment = false;
        // attachmentImageView intentionally not set — no depth
        framebuffers = std::make_unique<FramebufferManager>(fbConfig);

        // Load a fallback texture for ColoredQuad commands (shader ignores it
        // when useTexture == 0.0, but Vulkan requires a valid descriptor set).
        fallbackTextureID = resources->requestTexture(
            GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png");

        debugOverlay.init(resources);
        debugOverlay.setEnabled(debugEnabledByDefault);
    }

    GtsDebugOverlay& getDebugOverlay() { return debugOverlay; }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<UiCommandBuffer>(this);

        // Write-after-write serialization orders UiRenderStage after
        // SceneRenderStage — no explicit read declaration needed here.
        graph.declareWrite(this, outputHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            colorFinalLayout);
    }

    void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                uint32_t imageIndex, uint32_t currentFrame) override
    {
        UiCommandBuffer uiBuffer = graph.getData<UiCommandBuffer>();

        if (debugOverlay.isEnabled() && frameStats)
            debugOverlay.appendToBuffer(uiBuffer, *frameStats);

        if (uiBuffer.empty())
            return;

        // Upload vertices and indices.
        vertexBuffer.ensureCapacity(uiBuffer.vertices.size() * sizeof(UiVertex));
        indexBuffer.ensureCapacity(uiBuffer.indices.size()   * sizeof(uint32_t));

        std::memcpy(vertexBuffer.getMapped(),
                    uiBuffer.vertices.data(),
                    uiBuffer.vertices.size() * sizeof(UiVertex));
        std::memcpy(indexBuffer.getMapped(),
                    uiBuffer.indices.data(),
                    uiBuffer.indices.size() * sizeof(uint32_t));

        // Begin render pass.
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = renderPass->getRenderPass();
        rpInfo.framebuffer       = framebuffers->getFramebuffers()[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = vcsheet::getFrameOutputExtent();
        rpInfo.clearValueCount   = 0;
        rpInfo.pClearValues      = nullptr;
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.x        = 0.0f;
        vp.y        = 0.0f;
        vp.width    = static_cast<float>(vcsheet::getFrameOutputExtent().width);
        vp.height   = static_cast<float>(vcsheet::getFrameOutputExtent().height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vcsheet::getFrameOutputExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

        // DONT EVER CHANGE THIS, WE HAVE TO FLIP TOP AND BOT BECAUSE OF GLM SETTINGS, SO DONT CHANGE THIS
        glm::mat4 proj = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
        vkCmdPushConstants(cmd, pipeline->getPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(glm::mat4), &proj);

        const VkDeviceSize offsets[] = {0};
        VkBuffer vb = vertexBuffer.getBuffer();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
        VkBuffer ib = indexBuffer.getBuffer();
        vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);

        // One draw call per UiDrawCommand.
        for (const auto& drawCmd : uiBuffer.commands)
        {
            // Resolve texture — fall back for ColoredQuad (useTexture=0 in shader).
            texture_id_type resolvedID =
                (drawCmd.type == UiDrawType::TexturedQuad)
                    ? drawCmd.textureID
                    : fallbackTextureID;

            TextureResource* tex = resources->getTexture(resolvedID);
            if (!tex) tex = resources->getTexture(fallbackTextureID);
            if (!tex) continue;

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(), 0, 1,
                                    &tex->descriptorSets[currentFrame],
                                    0, nullptr);

            float useTexture = (drawCmd.type == UiDrawType::TexturedQuad) ? 1.0f : 0.0f;
            vkCmdPushConstants(cmd, pipeline->getPipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(glm::mat4), sizeof(float), &useTexture);

            vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.indexOffset, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

private:
    std::unique_ptr<VulkanRenderPass>     renderPass;
    std::unique_ptr<VulkanPipeline>       pipeline;
    std::unique_ptr<FramebufferManager>   framebuffers;
    RenderResourceManager*                resources;
    GtsResourceHandle                     outputHandle;
    texture_id_type                       fallbackTextureID = 0;
    GtsFrameStats*                        frameStats        = nullptr;
    GtsDebugOverlay                       debugOverlay;
    VkImageLayout                         colorInitialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkImageLayout                         colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Dynamic buffers — host-visible, persistently mapped, resized on demand.
    static constexpr uint32_t INITIAL_VERTEX_CAP = 1024;
    static constexpr uint32_t INITIAL_INDEX_CAP  = 2048;

    VulkanDynamicBuffer vertexBuffer{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                      INITIAL_VERTEX_CAP * sizeof(UiVertex)};
    VulkanDynamicBuffer indexBuffer {VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                      INITIAL_INDEX_CAP  * sizeof(uint32_t)};
};
