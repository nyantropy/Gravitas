#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include "GtsFrameGraph.h"
#include "GtsRenderStage.h"
#include "VulkanRenderPass.hpp"
#include "VulkanPipeline.hpp"
#include "FramebufferManager.hpp"
#include "RenderResourceManager.hpp"
#include "RenderCommand.h"
#include "ObjectUBO.h"
#include "vcsheet.h"

// Main 3D scene render stage.
// Encapsulates the scene render pass and all per-object draw calls.
class SceneRenderStage : public GtsRenderStage
{
public:
    SceneRenderStage(VulkanRenderPass*      renderPass,
                     VulkanPipeline*        pipeline,
                     FramebufferManager*    framebuffers,
                     RenderResourceManager* resources,
                     GtsResourceHandle      swapchainHandle,
                     GtsResourceHandle      depthHandle)
        : GtsRenderStage("SceneRenderStage")
        , renderPass(renderPass)
        , pipeline(pipeline)
        , framebuffers(framebuffers)
        , resources(resources)
        , swapchainHandle(swapchainHandle)
        , depthHandle(depthHandle)
    {}

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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

        // Bind set 0 (camera UBO) and set 1 (object SSBO) once for all draws.
        if (!renderList.empty() && renderList[0].cameraViewID != 0)
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

        const VkDeviceSize offsets[] = { 0 };

        for (const auto& cmdData : renderList)
        {
            MeshResource*    mesh = resources->getMesh(cmdData.meshID);
            TextureResource* tex  = resources->getTexture(cmdData.textureID);

            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Push objectIndex (vertex) + alpha (fragment) as a single 8-byte block.
            struct { uint32_t objectIndex; float alpha; } pc{ cmdData.objectSSBOSlot, cmdData.alpha };
            vkCmdPushConstants(cmd, pipeline->getPipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);

            // Bind set 2 (texture sampler) — changes per draw.
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(),
                                    2, 1, &tex->descriptorSets[currentFrame],
                                    0, nullptr);

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

private:
    VulkanRenderPass*      renderPass;
    VulkanPipeline*        pipeline;
    FramebufferManager*    framebuffers;
    RenderResourceManager* resources;
    GtsResourceHandle      swapchainHandle;
    GtsResourceHandle      depthHandle;
};
