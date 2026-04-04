#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "GtsFrameGraph.h"
#include "GtsRenderStage.h"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"
#include "VulkanPipeline.hpp"
#include "VulkanPipelineConfig.h"
#include "FramebufferManager.hpp"
#include "FramebufferManagerConfig.h"
#include "RenderResourceManager.hpp"
#include "RenderCommand.h"
#include "ObjectUBO.h"
#include "GraphicsConstants.h"
#include "vcsheet.h"

// Main 3D scene render stage.
// Owns its render pass, pipeline, and framebuffers.
// Borrows RenderResourceManager from ForwardRenderer (shared across stages).
class SceneRenderStage : public GtsRenderStage
{
public:
    SceneRenderStage(RenderResourceManager* resources,
                     VkImageView            depthImageView,
                     VkFormat               depthFormat,
                     GtsResourceHandle      swapchainHandle,
                     GtsResourceHandle      depthHandle)
        : GtsRenderStage("SceneRenderStage")
        , resources(resources)
        , swapchainHandle(swapchainHandle)
        , depthHandle(depthHandle)
    {
        // Render pass
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat = vcsheet::getSwapChainImageFormat();
        rpConfig.depthFormat = depthFormat;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        // Default pipeline — backface culling (cullMode defaults to VK_CULL_MODE_BACK_BIT)
        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath   = GraphicsConstants::V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::F_SHADER_PATH;
        pConfig.vkRenderPass       = renderPass->getRenderPass();
        pipeline = std::make_unique<VulkanPipeline>(pConfig);

        // Double-sided pipeline — no culling (planes, quads, world images)
        VulkanPipelineConfig pConfigDS = pConfig;
        pConfigDS.cullMode = VK_CULL_MODE_NONE;
        pipelineDoubleSided = std::make_unique<VulkanPipeline>(pConfigDS);

        // Framebuffers (color + depth)
        FramebufferManagerConfig fbConfig;
        fbConfig.attachmentImageView = depthImageView;
        fbConfig.vkRenderpass        = renderPass->getRenderPass();
        framebuffers = std::make_unique<FramebufferManager>(fbConfig);
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<std::vector<RenderCommand>>(this);

        // The scene render pass outputs the swapchain image in PRESENT_SRC_KHR
        // (the render pass finalLayout) and the depth in DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
        graph.declareWrite(this, swapchainHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        graph.declareWrite(this, depthHandle,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                uint32_t imageIndex, uint32_t currentFrame) override
    {
        const auto& renderList = graph.getData<std::vector<RenderCommand>>();

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = renderPass->getRenderPass();
        renderPassInfo.framebuffer       = framebuffers->getFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vcsheet::getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(vcsheet::getSwapChainExtent().width);
        viewport.height   = static_cast<float>(vcsheet::getSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vcsheet::getSwapChainExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // No active camera yet — skip all draws this frame.
        if (renderList.empty() || renderList[0].cameraViewID == 0)
        {
            vkCmdEndRenderPass(cmd);
            return;
        }

        // Bind set 0 (camera UBO) and set 1 (object SSBO) once for all draws.
        // Both pipeline variants share the same layout — use either layout here.
        {
            CameraBufferResource* cameraView = resources->getCameraView(renderList[0].cameraViewID);
            VkDescriptorSet globalSets[2] = {
                cameraView->descriptorSets[currentFrame],
                resources->getObjectSSBODescriptorSet(currentFrame)
            };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(),
                                    0, 2, globalSets, 0, nullptr);
        }

        // Sort render commands to group by (pipeline variant, mesh, texture), minimising
        // redundant Vulkan state changes.  We sort pointers to avoid copying RenderCommand
        // structs.  The render list is already stable_partitioned opaque-first by
        // RenderCommandExtractor, so we locate the partition point and sort only the opaque
        // prefix — transparent objects must remain in their original back-to-front order so
        // alpha blending is correct.
        std::vector<const RenderCommand*> sorted;
        sorted.reserve(renderList.size());
        for (const auto& rc : renderList)
            sorted.push_back(&rc);

        // Find where opaque commands end (renderList is opaque-first due to stable_partition).
        size_t opaqueCount = 0;
        while (opaqueCount < renderList.size() && renderList[opaqueCount].alpha >= 1.0f)
            ++opaqueCount;

        // Sort the opaque prefix by (doubleSided, meshID, textureID).
        // Transparent suffix stays in original back-to-front order.
        std::sort(sorted.begin(), sorted.begin() + opaqueCount,
            [](const RenderCommand* a, const RenderCommand* b)
            {
                if (a->doubleSided != b->doubleSided) return !a->doubleSided && b->doubleSided;
                if (a->meshID      != b->meshID)      return a->meshID      < b->meshID;
                return a->textureID < b->textureID;
            });

        const VkDeviceSize offsets[] = { 0 };

        // Bound-state cache: track what is currently set on the command buffer so
        // we skip redundant vkCmd* calls.  Initialise to sentinel values that can
        // never match a real resource.
        VulkanPipeline* boundPipeline = nullptr;
        mesh_id_type    boundMesh     = static_cast<mesh_id_type>(-1);
        texture_id_type boundTexture  = static_cast<texture_id_type>(-1);

        uint32_t triangles        = 0;
        uint32_t drawCalls        = 0;
        uint32_t pipelineSwitches = 0;
        uint32_t textureSwitches  = 0;

        for (const RenderCommand* cmdPtr : sorted)
        {
            const auto& cmdData = *cmdPtr;
            MeshResource*    mesh = resources->getMesh(cmdData.meshID);
            TextureResource* tex  = resources->getTexture(cmdData.textureID);

            triangles += static_cast<uint32_t>(mesh->indices.size()) / 3;

            VulkanPipeline* activePipeline = cmdData.doubleSided
                ? pipelineDoubleSided.get()
                : pipeline.get();

            if (activePipeline != boundPipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline->getPipeline());
                boundPipeline = activePipeline;
                ++pipelineSwitches;
            }

            if (cmdData.meshID != boundMesh)
            {
                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, offsets);
                vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                boundMesh = cmdData.meshID;
            }

            // Push objectIndex (vertex) + alpha (fragment) as a single 8-byte block.
            // objectSSBOSlot is unique per object so this always changes.
            struct { uint32_t objectIndex; float alpha; } pc{ cmdData.objectSSBOSlot, cmdData.alpha };
            vkCmdPushConstants(cmd, activePipeline->getPipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);

            if (cmdData.textureID != boundTexture)
            {
                // Bind set 2 (texture sampler) — only when it actually changes.
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        activePipeline->getPipelineLayout(),
                                        2, 1, &tex->descriptorSets[currentFrame],
                                        0, nullptr);
                boundTexture = cmdData.textureID;
                ++textureSwitches;
            }

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
            ++drawCalls;
        }

        lastTriangleCount   = triangles;
        lastDrawCalls       = drawCalls;
        lastPipelineSwitches = pipelineSwitches;
        lastTextureSwitches = textureSwitches;

        vkCmdEndRenderPass(cmd);
    }

    uint32_t getLastTriangleCount()    const { return lastTriangleCount; }
    uint32_t getLastDrawCalls()        const { return lastDrawCalls; }
    uint32_t getLastPipelineSwitches() const { return lastPipelineSwitches; }
    uint32_t getLastTextureSwitches()  const { return lastTextureSwitches; }

private:
    std::unique_ptr<VulkanRenderPass>   renderPass;
    std::unique_ptr<VulkanPipeline>     pipeline;            // backface culled (default)
    std::unique_ptr<VulkanPipeline>     pipelineDoubleSided; // no culling
    std::unique_ptr<FramebufferManager> framebuffers;
    RenderResourceManager*              resources;
    GtsResourceHandle                   swapchainHandle;
    GtsResourceHandle                   depthHandle;
    uint32_t                            lastTriangleCount    = 0;
    uint32_t                            lastDrawCalls        = 0;
    uint32_t                            lastPipelineSwitches = 0;
    uint32_t                            lastTextureSwitches  = 0;

};
