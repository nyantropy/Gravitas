#pragma once

#include <algorithm>
#include <array>
#include <cstring>
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
#include "EngineConfig.h"
#include "BufferUtil.hpp"
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
                     GtsResourceHandle      outputHandle,
                     GtsResourceHandle      depthHandle,
                     VkImageLayout          colorFinalLayout)
        : GtsRenderStage("SceneRenderStage")
        , resources(resources)
        , outputHandle(outputHandle)
        , depthHandle(depthHandle)
        , colorFinalLayout(colorFinalLayout)
    {
        // Render pass
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat = vcsheet::getFrameOutputFormat();
        rpConfig.depthFormat = depthFormat;
        rpConfig.colorFinalLayout = colorFinalLayout;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        // Vertex input: binding 0 = per-vertex Vertex, binding 1 = per-instance objectSSBOSlot (uint32).
        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding   = 1;
        instanceBinding.stride    = sizeof(uint32_t);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        VkVertexInputAttributeDescription instanceAttr{};
        instanceAttr.binding  = 1;
        instanceAttr.location = 3;
        instanceAttr.format   = VK_FORMAT_R32_UINT;
        instanceAttr.offset   = 0;

        // Default pipeline — backface culling (cullMode defaults to VK_CULL_MODE_BACK_BIT)
        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath   = GraphicsConstants::V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::F_SHADER_PATH;
        pConfig.vkRenderPass       = renderPass->getRenderPass();
        pConfig.vertexBindings.push_back(instanceBinding);
        pConfig.vertexAttributes.push_back(instanceAttr);
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

        // Per-frame host-visible instance data buffers (objectSSBOSlot per instance).
        // Each frame gets its own buffer so the CPU can write frame N while the GPU
        // reads frame N-1.
        const VkDeviceSize instanceBufSize =
            static_cast<VkDeviceSize>(EngineConfig::MAX_RENDERABLE_OBJECTS) * sizeof(uint32_t);
        for (uint32_t f = 0; f < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++f)
        {
            BufferUtil::createBuffer(
                vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
                instanceBufSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                instanceBuffers[f], instanceBufferMemory[f]);
            vkMapMemory(vcsheet::getDevice(), instanceBufferMemory[f],
                        0, instanceBufSize, 0, &instanceBufferMapped[f]);
        }
    }

    ~SceneRenderStage()
    {
        for (uint32_t f = 0; f < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++f)
        {
            if (instanceBufferMapped[f])
                vkUnmapMemory(vcsheet::getDevice(), instanceBufferMemory[f]);
            if (instanceBuffers[f] != VK_NULL_HANDLE)
                vkDestroyBuffer(vcsheet::getDevice(), instanceBuffers[f], nullptr);
            if (instanceBufferMemory[f] != VK_NULL_HANDLE)
                vkFreeMemory(vcsheet::getDevice(), instanceBufferMemory[f], nullptr);
        }
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<std::vector<RenderCommand>>(this);

        // The scene render pass outputs the frame image in the layout chosen by the
        // active frame output target and the depth in DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
        graph.declareWrite(this, outputHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            colorFinalLayout);

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
        renderPassInfo.renderArea.extent = vcsheet::getFrameOutputExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(vcsheet::getFrameOutputExtent().width);
        viewport.height   = static_cast<float>(vcsheet::getFrameOutputExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vcsheet::getFrameOutputExtent();
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

        const VkDeviceSize zeroOffset = 0;

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

        // Per-frame host-visible instance data buffer: objectSSBOSlot for each instance.
        // We fill it in batch order and reference sub-ranges via vkCmdBindVertexBuffers offset.
        auto* instanceData    = static_cast<uint32_t*>(instanceBufferMapped[currentFrame]);
        uint32_t instanceHead = 0; // next free slot in instanceData[]

        // Iterate sorted commands, grouping consecutive same-(pipeline, mesh, texture, alpha)
        // commands into one instanced draw call.
        size_t i = 0;
        while (i < renderList.size())
        {
            const RenderCommand* first = &renderList[i];
            MeshResource*    mesh = resources->getMesh(first->meshID);
            TextureResource* tex  = resources->getTexture(first->textureID);
            VulkanPipeline*  activePipeline = first->doubleSided
                ? pipelineDoubleSided.get()
                : pipeline.get();

            // Collect all consecutive commands that can share one draw call.
            const uint32_t batchBase  = instanceHead;
            uint32_t       batchCount = 0;
            while (i < renderList.size())
            {
                const RenderCommand* cur = &renderList[i];
                VulkanPipeline* curPipeline = cur->doubleSided
                    ? pipelineDoubleSided.get()
                    : pipeline.get();
                if (curPipeline      != activePipeline
                    || cur->meshID    != first->meshID
                    || cur->textureID != first->textureID
                    || cur->alpha     != first->alpha)
                    break;
                instanceData[instanceHead + batchCount] = cur->objectSSBOSlot;
                ++batchCount;
                ++i;
            }
            instanceHead += batchCount;

            // Vulkan state — bind only on change.
            if (activePipeline != boundPipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  activePipeline->getPipeline());
                boundPipeline = activePipeline;
                ++pipelineSwitches;
            }

            if (first->meshID != boundMesh)
            {
                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, &zeroOffset);
                vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                boundMesh = first->meshID;
            }

            if (first->textureID != boundTexture)
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        activePipeline->getPipelineLayout(),
                                        2, 1, &tex->descriptorSets[currentFrame],
                                        0, nullptr);
                boundTexture = first->textureID;
                ++textureSwitches;
            }

            // Push alpha for the fragment stage (same for all instances in this batch).
            float alpha = first->alpha;
            vkCmdPushConstants(cmd, activePipeline->getPipelineLayout(),
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(float), &alpha);

            // Bind the instance slice of the per-frame instance buffer as vertex binding 1.
            VkDeviceSize instanceBufOffset = batchBase * sizeof(uint32_t);
            vkCmdBindVertexBuffers(cmd, 1, 1, &instanceBuffers[currentFrame], &instanceBufOffset);

            // One instanced draw for the whole batch.
            const uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
            vkCmdDrawIndexed(cmd, indexCount, batchCount, 0, 0, 0);

            triangles += (indexCount / 3) * batchCount;
            ++drawCalls;
        }

        lastTriangleCount    = triangles;
        lastDrawCalls        = drawCalls;
        lastPipelineSwitches = pipelineSwitches;
        lastTextureSwitches  = textureSwitches;

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
    GtsResourceHandle                   outputHandle;
    GtsResourceHandle                   depthHandle;
    VkImageLayout                       colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Per-frame host-visible instance data buffers: objectSSBOSlot (uint32) per instance.
    // Filled each frame before recording; one buffer per frame in flight so CPU writes
    // to frame N while the GPU reads frame N-1.
    std::array<VkBuffer,       GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBuffers{};
    std::array<VkDeviceMemory, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBufferMemory{};
    std::array<void*,          GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBufferMapped{};

    uint32_t                            lastTriangleCount    = 0;
    uint32_t                            lastDrawCalls        = 0;
    uint32_t                            lastPipelineSwitches = 0;
    uint32_t                            lastTextureSwitches  = 0;

};
