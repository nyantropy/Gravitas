#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>

#include "threading/ThreadPool.h"
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

class SceneRenderStage : public GtsRenderStage
{
public:
    SceneRenderStage(RenderResourceManager* resources,
                     ThreadPool*            threadPool,
                     VkImageView            depthImageView,
                     VkFormat               depthFormat,
                     GtsResourceHandle      outputHandle,
                     GtsResourceHandle      depthHandle,
                     VkImageLayout          colorFinalLayout)
        : GtsRenderStage("SceneRenderStage")
        , resources(resources)
        , threadPool(threadPool)
        , outputHandle(outputHandle)
        , depthHandle(depthHandle)
        , colorFinalLayout(colorFinalLayout)
    {
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat = vcsheet::getFrameOutputFormat();
        rpConfig.depthFormat = depthFormat;
        rpConfig.colorFinalLayout = colorFinalLayout;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding   = 1;
        instanceBinding.stride    = sizeof(uint32_t);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        VkVertexInputAttributeDescription instanceAttr{};
        instanceAttr.binding  = 1;
        instanceAttr.location = 3;
        instanceAttr.format   = VK_FORMAT_R32_UINT;
        instanceAttr.offset   = 0;

        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath   = GraphicsConstants::V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::F_SHADER_PATH;
        pConfig.vkRenderPass       = renderPass->getRenderPass();
        pConfig.vertexBindings.push_back(instanceBinding);
        pConfig.vertexAttributes.push_back(instanceAttr);
        pipeline = std::make_unique<VulkanPipeline>(pConfig);

        VulkanPipelineConfig pConfigDS = pConfig;
        pConfigDS.cullMode = VK_CULL_MODE_NONE;
        pipelineDoubleSided = std::make_unique<VulkanPipeline>(pConfigDS);

        FramebufferManagerConfig fbConfig;
        fbConfig.attachmentImageView = depthImageView;
        fbConfig.vkRenderpass        = renderPass->getRenderPass();
        framebuffers = std::make_unique<FramebufferManager>(fbConfig);

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

        initializeParallelRecordingResources();

        preparedBatches.reserve(EngineConfig::MAX_RENDERABLE_OBJECTS);
        chunkRanges.reserve(maxRecordingSlots);
        secondaryExecutionBuffers.resize(maxRecordingSlots, VK_NULL_HANDLE);
        chunkStats.resize(maxRecordingSlots);
    }

    ~SceneRenderStage()
    {
        destroyParallelRecordingResources();

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

        prepareBatches(renderList, currentFrame);
        resetFrameStats();

        if (renderList.empty() || renderList[0].cameraViewID == 0)
        {
            recordInline(cmd, imageIndex, currentFrame, renderList);
            return;
        }

        resetSecondaryCommandPools(currentFrame);

        if (shouldRecordInParallel(renderList))
            recordParallel(cmd, imageIndex, currentFrame, renderList);
        else
            recordInline(cmd, imageIndex, currentFrame, renderList);
    }

    uint32_t getLastTriangleCount() const
    {
        return lastTriangleCount;
    }

    uint32_t getLastDrawCalls() const
    {
        return lastDrawCalls;
    }

    uint32_t getLastPipelineSwitches() const
    {
        return lastPipelineSwitches;
    }

    uint32_t getLastTextureSwitches() const
    {
        return lastTextureSwitches;
    }

private:
    struct PreparedBatch
    {
        mesh_id_type    meshID = 0;
        texture_id_type textureID = 0;
        float           alpha = 1.0f;
        bool            doubleSided = false;
        uint32_t        instanceOffset = 0;
        uint32_t        instanceCount = 0;
        MeshResource*   mesh = nullptr;
        TextureResource* texture = nullptr;
    };

    struct BatchRange
    {
        uint32_t start = 0;
        uint32_t end = 0;
    };

    struct ChunkStats
    {
        uint32_t triangles = 0;
        uint32_t drawCalls = 0;
        uint32_t pipelineSwitches = 0;
        uint32_t textureSwitches = 0;
    };

    struct GlobalDescriptorSets
    {
        VkDescriptorSet camera = VK_NULL_HANDLE;
        VkDescriptorSet object = VK_NULL_HANDLE;
    };

    static constexpr uint32_t PARALLEL_RECORDING_THRESHOLD = 64;

    std::unique_ptr<VulkanRenderPass>   renderPass;
    std::unique_ptr<VulkanPipeline>     pipeline;
    std::unique_ptr<VulkanPipeline>     pipelineDoubleSided;
    std::unique_ptr<FramebufferManager> framebuffers;
    RenderResourceManager*              resources = nullptr;
    ThreadPool*                         threadPool = nullptr;
    GtsResourceHandle                   outputHandle;
    GtsResourceHandle                   depthHandle;
    VkImageLayout                       colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    std::array<VkBuffer,       GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBuffers{};
    std::array<VkDeviceMemory, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBufferMemory{};
    std::array<void*,          GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBufferMapped{};

    uint32_t                            lastTriangleCount = 0;
    uint32_t                            lastDrawCalls = 0;
    uint32_t                            lastPipelineSwitches = 0;
    uint32_t                            lastTextureSwitches = 0;

    uint32_t                            maxRecordingSlots = 1;
    std::vector<std::vector<VkCommandPool>>   secondaryCommandPools;
    std::vector<std::vector<VkCommandBuffer>> secondaryCommandBuffers;
    std::vector<PreparedBatch>          preparedBatches;
    std::vector<BatchRange>             chunkRanges;
    std::vector<VkCommandBuffer>        secondaryExecutionBuffers;
    std::vector<ChunkStats>             chunkStats;

    void initializeParallelRecordingResources()
    {
        maxRecordingSlots = (threadPool ? threadPool->threadCount() : 0u) + 1u;
        secondaryCommandPools.assign(
            GraphicsConstants::MAX_FRAMES_IN_FLIGHT,
            std::vector<VkCommandPool>(maxRecordingSlots, VK_NULL_HANDLE));
        secondaryCommandBuffers.assign(
            GraphicsConstants::MAX_FRAMES_IN_FLIGHT,
            std::vector<VkCommandBuffer>(maxRecordingSlots, VK_NULL_HANDLE));

        QueueFamilyIndices indices = vcsheet::getQueueFamilyIndices();
        for (uint32_t frameIndex = 0; frameIndex < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            for (uint32_t slot = 0; slot < maxRecordingSlots; ++slot)
            {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

                if (vkCreateCommandPool(vcsheet::getDevice(), &poolInfo, nullptr,
                                        &secondaryCommandPools[frameIndex][slot]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create secondary command pool");
                }

                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = secondaryCommandPools[frameIndex][slot];
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocInfo.commandBufferCount = 1;

                if (vkAllocateCommandBuffers(vcsheet::getDevice(), &allocInfo,
                                             &secondaryCommandBuffers[frameIndex][slot]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to allocate secondary command buffer");
                }
            }
        }
    }

    void destroyParallelRecordingResources()
    {
        for (uint32_t frameIndex = 0; frameIndex < secondaryCommandPools.size(); ++frameIndex)
        {
            for (uint32_t slot = 0; slot < secondaryCommandPools[frameIndex].size(); ++slot)
            {
                if (secondaryCommandPools[frameIndex][slot] != VK_NULL_HANDLE)
                {
                    vkDestroyCommandPool(vcsheet::getDevice(),
                                         secondaryCommandPools[frameIndex][slot],
                                         nullptr);
                }
            }
        }
    }

    void resetFrameStats()
    {
        lastTriangleCount = 0;
        lastDrawCalls = 0;
        lastPipelineSwitches = 0;
        lastTextureSwitches = 0;
    }

    void resetSecondaryCommandPools(uint32_t currentFrame)
    {
        for (uint32_t slot = 0; slot < maxRecordingSlots; ++slot)
        {
            vkResetCommandPool(vcsheet::getDevice(),
                               secondaryCommandPools[currentFrame][slot],
                               0);
        }
    }

    VkRenderPassBeginInfo makeRenderPassBeginInfo(uint32_t imageIndex, std::array<VkClearValue, 2>& clearValues) const
    {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass->getRenderPass();
        renderPassInfo.framebuffer = framebuffers->getFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vcsheet::getFrameOutputExtent();

        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        return renderPassInfo;
    }

    void setViewportAndScissor(VkCommandBuffer cmd) const
    {
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(vcsheet::getFrameOutputExtent().width);
        viewport.height = static_cast<float>(vcsheet::getFrameOutputExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vcsheet::getFrameOutputExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    bool shouldRecordInParallel(const std::vector<RenderCommand>& renderList) const
    {
        if (!threadPool)
            return false;

        if (threadPool->threadCount() == 0)
            return false;

        if (renderList.size() < PARALLEL_RECORDING_THRESHOLD)
            return false;

        return preparedBatches.size() > 1;
    }

    void prepareBatches(const std::vector<RenderCommand>& renderList, uint32_t currentFrame)
    {
        preparedBatches.clear();

        if (renderList.empty())
            return;

        auto* instanceData = static_cast<uint32_t*>(instanceBufferMapped[currentFrame]);
        uint32_t instanceHead = 0;
        size_t i = 0;

        while (i < renderList.size())
        {
            const RenderCommand& first = renderList[i];
            PreparedBatch batch{};
            batch.meshID = first.meshID;
            batch.textureID = first.textureID;
            batch.alpha = first.alpha;
            batch.doubleSided = first.doubleSided;
            batch.instanceOffset = instanceHead;
            batch.mesh = resources->getMesh(first.meshID);
            batch.texture = resources->getTexture(first.textureID);

            while (i < renderList.size())
            {
                const RenderCommand& current = renderList[i];
                if (current.meshID != batch.meshID
                    || current.textureID != batch.textureID
                    || current.alpha != batch.alpha
                    || current.doubleSided != batch.doubleSided)
                {
                    break;
                }

                instanceData[instanceHead] = current.objectSSBOSlot;
                instanceHead += 1;
                batch.instanceCount += 1;
                i += 1;
            }

            preparedBatches.push_back(batch);
        }
    }

    GlobalDescriptorSets resolveGlobalDescriptorSets(uint32_t currentFrame, view_id_type cameraViewID) const
    {
        CameraBufferResource* cameraView = resources->getCameraView(cameraViewID);
        return {
            cameraView->descriptorSets[currentFrame],
            resources->getObjectSSBODescriptorSet(currentFrame)
        };
    }

    void bindGlobalDescriptorSets(VkCommandBuffer cmd, const GlobalDescriptorSets& globalSets) const
    {
        VkDescriptorSet sets[2] = {globalSets.camera, globalSets.object};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(),
                                0, 2, sets, 0, nullptr);
    }

    void recordBatchRange(VkCommandBuffer cmd,
                          uint32_t currentFrame,
                          uint32_t batchStart,
                          uint32_t batchEnd,
                          ChunkStats& stats) const
    {
        const VkDeviceSize zeroOffset = 0;
        VulkanPipeline* boundPipeline = nullptr;
        mesh_id_type boundMesh = static_cast<mesh_id_type>(-1);
        texture_id_type boundTexture = static_cast<texture_id_type>(-1);

        for (uint32_t batchIndex = batchStart; batchIndex < batchEnd; ++batchIndex)
        {
            const PreparedBatch& batch = preparedBatches[batchIndex];
            MeshResource* mesh = batch.mesh;
            TextureResource* tex = batch.texture;
            VulkanPipeline* activePipeline = batch.doubleSided
                ? pipelineDoubleSided.get()
                : pipeline.get();

            if (activePipeline != boundPipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline->getPipeline());
                boundPipeline = activePipeline;
                stats.pipelineSwitches += 1;
            }

            if (batch.meshID != boundMesh)
            {
                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, &zeroOffset);
                vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                boundMesh = batch.meshID;
            }

            if (batch.textureID != boundTexture)
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        activePipeline->getPipelineLayout(),
                                        2, 1, &tex->descriptorSets[currentFrame],
                                        0, nullptr);
                boundTexture = batch.textureID;
                stats.textureSwitches += 1;
            }

            float alpha = batch.alpha;
            vkCmdPushConstants(cmd, activePipeline->getPipelineLayout(),
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(float), &alpha);

            VkDeviceSize instanceOffset = static_cast<VkDeviceSize>(batch.instanceOffset) * sizeof(uint32_t);
            vkCmdBindVertexBuffers(cmd, 1, 1, &instanceBuffers[currentFrame], &instanceOffset);

            const uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
            vkCmdDrawIndexed(cmd, indexCount, batch.instanceCount, 0, 0, 0);

            stats.triangles += (indexCount / 3) * batch.instanceCount;
            stats.drawCalls += 1;
        }
    }

    void aggregateChunkStats(uint32_t chunkCount)
    {
        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            lastTriangleCount += chunkStats[i].triangles;
            lastDrawCalls += chunkStats[i].drawCalls;
            lastPipelineSwitches += chunkStats[i].pipelineSwitches;
            lastTextureSwitches += chunkStats[i].textureSwitches;
        }
    }

    uint32_t buildChunkRanges(const std::vector<RenderCommand>& renderList)
    {
        chunkRanges.clear();

        const uint32_t batchCount = static_cast<uint32_t>(preparedBatches.size());
        if (batchCount == 0)
            return 0;

        const uint32_t desiredChunks = std::min(maxRecordingSlots, batchCount);
        uint32_t nextBatch = 0;
        uint32_t remainingCommands = static_cast<uint32_t>(renderList.size());
        uint32_t remainingChunks = desiredChunks;

        while (nextBatch < batchCount && remainingChunks > 0)
        {
            const uint32_t targetCommands = (remainingCommands + remainingChunks - 1) / remainingChunks;
            uint32_t accumulatedCommands = 0;
            const uint32_t startBatch = nextBatch;

            while (nextBatch < batchCount)
            {
                const uint32_t batchCommands = preparedBatches[nextBatch].instanceCount;
                const uint32_t batchesLeftAfterThis = batchCount - (nextBatch + 1);
                const uint32_t chunksLeftAfterThis = remainingChunks - 1;

                if (accumulatedCommands > 0
                    && accumulatedCommands + batchCommands > targetCommands
                    && batchesLeftAfterThis >= chunksLeftAfterThis)
                {
                    break;
                }

                accumulatedCommands += batchCommands;
                nextBatch += 1;

                if (batchesLeftAfterThis < chunksLeftAfterThis)
                    break;
            }

            chunkRanges.push_back({startBatch, nextBatch});
            remainingCommands -= accumulatedCommands;
            remainingChunks -= 1;
        }

        return static_cast<uint32_t>(chunkRanges.size());
    }

    void beginSecondaryCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) const
    {
        VkCommandBufferInheritanceInfo inheritance{};
        inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritance.renderPass = renderPass->getRenderPass();
        inheritance.subpass = 0;
        inheritance.framebuffer = framebuffers->getFramebuffers()[imageIndex];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo = &inheritance;

        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("failed to begin secondary command buffer");
    }

    void recordInline(VkCommandBuffer cmd,
                      uint32_t imageIndex,
                      uint32_t currentFrame,
                      const std::vector<RenderCommand>& renderList)
    {
        std::array<VkClearValue, 2> clearValues{};
        VkRenderPassBeginInfo renderPassInfo = makeRenderPassBeginInfo(imageIndex, clearValues);
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        setViewportAndScissor(cmd);

        if (preparedBatches.empty() || renderList[0].cameraViewID == 0)
        {
            vkCmdEndRenderPass(cmd);
            return;
        }

        const GlobalDescriptorSets globalSets =
            resolveGlobalDescriptorSets(currentFrame, renderList[0].cameraViewID);
        bindGlobalDescriptorSets(cmd, globalSets);
        ChunkStats inlineStats{};
        recordBatchRange(cmd, currentFrame, 0, static_cast<uint32_t>(preparedBatches.size()), inlineStats);
        chunkStats[0] = inlineStats;
        aggregateChunkStats(1);
        vkCmdEndRenderPass(cmd);
    }

    void recordParallel(VkCommandBuffer cmd,
                        uint32_t imageIndex,
                        uint32_t currentFrame,
                        const std::vector<RenderCommand>& renderList)
    {
        const uint32_t chunkCount = buildChunkRanges(renderList);
        if (chunkCount == 0)
        {
            recordInline(cmd, imageIndex, currentFrame, renderList);
            return;
        }

        const view_id_type cameraViewID = renderList[0].cameraViewID;
        const GlobalDescriptorSets globalSets = resolveGlobalDescriptorSets(currentFrame, cameraViewID);

        for (uint32_t i = 0; i < chunkCount; ++i)
            chunkStats[i] = {};

        const uint32_t mainThreadSlot = maxRecordingSlots - 1;

        threadPool->parallelFor(chunkCount, [&](uint32_t chunkIndex)
        {
            const uint32_t slot = (chunkIndex + 1 == chunkCount) ? mainThreadSlot : chunkIndex;
            VkCommandBuffer secondary = secondaryCommandBuffers[currentFrame][slot];
            secondaryExecutionBuffers[chunkIndex] = secondary;

            beginSecondaryCommandBuffer(secondary, imageIndex);
            setViewportAndScissor(secondary);
            bindGlobalDescriptorSets(secondary, globalSets);
            recordBatchRange(secondary, currentFrame,
                             chunkRanges[chunkIndex].start,
                             chunkRanges[chunkIndex].end,
                             chunkStats[chunkIndex]);

            if (vkEndCommandBuffer(secondary) != VK_SUCCESS)
                throw std::runtime_error("failed to end secondary command buffer");
        });

        std::array<VkClearValue, 2> clearValues{};
        VkRenderPassBeginInfo renderPassInfo = makeRenderPassBeginInfo(imageIndex, clearValues);
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkCmdExecuteCommands(cmd, chunkCount, secondaryExecutionBuffers.data());
        vkCmdEndRenderPass(cmd);

        aggregateChunkStats(chunkCount);
    }
};
