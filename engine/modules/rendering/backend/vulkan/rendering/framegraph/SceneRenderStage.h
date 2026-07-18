#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
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
#include "RenderViewport.h"
#include "ObjectUBO.h"
#include "GraphicsConstants.h"
#include "BufferUtil.hpp"
#include "DescriptorSetManager.hpp"
#include "EditorPreviewRenderData.h"
#include "VulkanBackendContext.h"

class SceneRenderStage : public GtsRenderStage
{
    struct ScenePushConstants
    {
        // x = vertex-color-only, y = material feature flags.
        glm::ivec4 materialFlags = {0, 0, 0, 0};
        // Shared material base-color factor.
        glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        // x = metallic, y = roughness, z = normalScale, w = AO strength.
        glm::vec4 surfaceFactors = {0.0f, 1.0f, 1.0f, 1.0f};
        // xyz = emissive factor, w = emissive strength.
        glm::vec4 emissiveFactorStrength = {0.0f, 0.0f, 0.0f, 1.0f};
    };

public:
    enum class DataSource
    {
        RuntimeWorld,
        EditorPreview
    };

    SceneRenderStage(RenderResourceManager* resources,
                     ThreadPool*            threadPool,
                     VulkanBackendContext&  backendContext,
                     DescriptorSetManager&  descriptorSetManager,
                     const std::vector<VkImageView>& colorImageViews,
                     uint32_t               framebufferCount,
                     VkFormat               colorFormat,
                     VkExtent2D             renderExtent,
                     VkImageView            depthImageView,
                     VkFormat               depthFormat,
                     GtsResourceHandle      outputHandle,
                     GtsResourceHandle      depthHandle,
                     VkImageLayout          colorFinalLayout,
                     DataSource             dataSource = DataSource::RuntimeWorld)
        : GtsRenderStage("SceneRenderStage")
        , resources(resources)
        , threadPool(threadPool)
        , backendContext(backendContext)
        , descriptorSetManager(descriptorSetManager)
        , renderExtent(renderExtent)
        , outputHandle(outputHandle)
        , depthHandle(depthHandle)
        , colorFinalLayout(colorFinalLayout)
        , dataSource(dataSource)
    {
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat = colorFormat;
        rpConfig.depthFormat = depthFormat;
        rpConfig.colorFinalLayout = colorFinalLayout;
        renderPass = std::make_unique<VulkanRenderPass>(backendContext, rpConfig);

        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding   = 1;
        instanceBinding.stride    = sizeof(uint32_t);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        VkVertexInputAttributeDescription instanceAttr{};
        instanceAttr.binding  = 1;
        instanceAttr.location = 5;
        instanceAttr.format   = VK_FORMAT_R32_UINT;
        instanceAttr.offset   = 0;

        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath   = GraphicsConstants::V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::F_SHADER_PATH;
        pConfig.vkRenderPass       = renderPass->getRenderPass();
        pConfig.pushConstantSize   = sizeof(ScenePushConstants);
        pConfig.vertexBindings.push_back(instanceBinding);
        pConfig.vertexAttributes.push_back(instanceAttr);
        pipeline = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pConfig);

        VulkanPipelineConfig pConfigDS = pConfig;
        pConfigDS.cullMode = VK_CULL_MODE_NONE;
        pipelineDoubleSided = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pConfigDS);

        VulkanPipelineConfig pConfigAlphaNoDepth = pConfig;
        pConfigAlphaNoDepth.depthWriteEnable = false;
        pipelineAlphaNoDepth = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pConfigAlphaNoDepth);

        VulkanPipelineConfig pConfigAlphaNoDepthDS = pConfigAlphaNoDepth;
        pConfigAlphaNoDepthDS.cullMode = VK_CULL_MODE_NONE;
        pipelineAlphaNoDepthDoubleSided = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pConfigAlphaNoDepthDS);

        VulkanPipelineConfig pConfigAdditiveNoDepth = pConfigAlphaNoDepth;
        pConfigAdditiveNoDepth.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pConfigAdditiveNoDepth.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pConfigAdditiveNoDepth.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pConfigAdditiveNoDepth.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineAdditiveNoDepth = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pConfigAdditiveNoDepth);

        VulkanPipelineConfig pConfigAdditiveNoDepthDS = pConfigAdditiveNoDepth;
        pConfigAdditiveNoDepthDS.cullMode = VK_CULL_MODE_NONE;
        pipelineAdditiveNoDepthDoubleSided = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pConfigAdditiveNoDepthDS);

        VulkanPipelineConfig pbrConfig = pConfig;
        pbrConfig.fragmentShaderPath = GraphicsConstants::PBR_F_SHADER_PATH;
        pipelineLit = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pbrConfig);

        VulkanPipelineConfig pbrConfigDS = pbrConfig;
        pbrConfigDS.cullMode = VK_CULL_MODE_NONE;
        pipelineLitDoubleSided = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pbrConfigDS);

        VulkanPipelineConfig pbrConfigAlphaNoDepth = pbrConfig;
        pbrConfigAlphaNoDepth.depthWriteEnable = false;
        pipelineLitAlphaNoDepth = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pbrConfigAlphaNoDepth);

        VulkanPipelineConfig pbrConfigAlphaNoDepthDS = pbrConfigAlphaNoDepth;
        pbrConfigAlphaNoDepthDS.cullMode = VK_CULL_MODE_NONE;
        pipelineLitAlphaNoDepthDoubleSided =
            std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pbrConfigAlphaNoDepthDS);

        VulkanPipelineConfig pbrConfigAdditiveNoDepth = pbrConfigAlphaNoDepth;
        pbrConfigAdditiveNoDepth.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pbrConfigAdditiveNoDepth.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pbrConfigAdditiveNoDepth.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pbrConfigAdditiveNoDepth.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineLitAdditiveNoDepth =
            std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pbrConfigAdditiveNoDepth);

        VulkanPipelineConfig pbrConfigAdditiveNoDepthDS = pbrConfigAdditiveNoDepth;
        pbrConfigAdditiveNoDepthDS.cullMode = VK_CULL_MODE_NONE;
        pipelineLitAdditiveNoDepthDoubleSided =
            std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, pbrConfigAdditiveNoDepthDS);

        FramebufferManagerConfig fbConfig;
        fbConfig.colorImageViews     = colorImageViews;
        fbConfig.attachmentImageView = depthImageView;
        fbConfig.vkRenderpass        = renderPass->getRenderPass();
        fbConfig.width               = renderExtent.width;
        fbConfig.height              = renderExtent.height;
        fbConfig.framebufferCount    = framebufferCount;
        framebuffers = std::make_unique<FramebufferManager>(backendContext, fbConfig);

        const VkDeviceSize instanceBufSize =
            static_cast<VkDeviceSize>(GraphicsConstants::MAX_RENDERABLE_OBJECTS) * sizeof(uint32_t);
        for (uint32_t f = 0; f < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++f)
        {
            BufferUtil::createBuffer(
                backendContext.device(), backendContext.physicalDevice(),
                instanceBufSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                instanceBuffers[f], instanceBufferMemory[f]);
            vkMapMemory(backendContext.device(), instanceBufferMemory[f],
                        0, instanceBufSize, 0, &instanceBufferMapped[f]);
        }

        initializeParallelRecordingResources();

        preparedBatches.reserve(GraphicsConstants::MAX_RENDERABLE_OBJECTS);
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
                vkUnmapMemory(backendContext.device(), instanceBufferMemory[f]);
            if (instanceBuffers[f] != VK_NULL_HANDLE)
                vkDestroyBuffer(backendContext.device(), instanceBuffers[f], nullptr);
            if (instanceBufferMemory[f] != VK_NULL_HANDLE)
                vkFreeMemory(backendContext.device(), instanceBufferMemory[f], nullptr);
        }
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        if (dataSource == DataSource::EditorPreview)
            graph.requestData<EditorPreviewRenderData>(this);
        else
        {
            graph.requestData<std::vector<RenderCommand>>(this);
            graph.requestData<MaterialFrameData>(this);
            graph.requestData<RenderViewportFrame>(this);
        }

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
        const std::vector<RenderCommand>& renderList = resolveRenderList(graph);
        const MaterialFrameData& materialFrameData = resolveMaterialFrameData(graph);
        const RenderViewportRect viewport = resolveViewport(graph);

        prepareBatches(renderList, materialFrameData, currentFrame);
        resetFrameStats();

        if (renderList.empty() || renderList[0].cameraViewID == 0)
        {
            recordInline(cmd, imageIndex, currentFrame, renderList, viewport);
            return;
        }

        if (shouldRecordInParallel(renderList))
        {
            resetSecondaryCommandPools(currentFrame);
            recordParallel(cmd, imageIndex, currentFrame, renderList, viewport);
        }
        else
        {
            recordInline(cmd, imageIndex, currentFrame, renderList, viewport);
        }
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

    uint32_t getLastPipelineBinds() const
    {
        return lastPipelineBinds;
    }

    uint32_t getLastDescriptorBinds() const
    {
        return lastDescriptorBinds;
    }

    uint32_t getLastTextureSwitches() const
    {
        return lastTextureSwitches;
    }

private:
    struct PreparedBatch
    {
        mesh_id_type meshID = 0;
        MaterialGpuHandle materialGpu;
        MaterialVariantKey variantKey{};
        RenderQueue renderQueue = RenderQueue::Opaque;
        MaterialFrameState material{};
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        uint32_t instanceOffset = 0;
        uint32_t instanceCount = 0;
        MeshResource* mesh = nullptr;
        const std::vector<VkDescriptorSet>* materialTextureSets = nullptr;
        bool litCompatible = true;
        bool normalMapCompatible = true;
        MaterialFeatureFlags effectiveFeatureFlags = MaterialFeatureFlags::None;
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
        uint32_t pipelineBinds = 0;
        uint32_t descriptorBinds = 0;
        uint32_t pipelineSwitches = 0;
        uint32_t textureSwitches = 0;
    };

    struct GlobalDescriptorSets
    {
        VkDescriptorSet camera = VK_NULL_HANDLE;
        VkDescriptorSet object = VK_NULL_HANDLE;
        VkDescriptorSet environment = VK_NULL_HANDLE;
    };

    static constexpr uint32_t PARALLEL_RECORDING_THRESHOLD = 64;

    std::unique_ptr<VulkanRenderPass>   renderPass;
    std::unique_ptr<VulkanPipeline>     pipeline;
    std::unique_ptr<VulkanPipeline>     pipelineDoubleSided;
    std::unique_ptr<VulkanPipeline>     pipelineAlphaNoDepth;
    std::unique_ptr<VulkanPipeline>     pipelineAlphaNoDepthDoubleSided;
    std::unique_ptr<VulkanPipeline>     pipelineAdditiveNoDepth;
    std::unique_ptr<VulkanPipeline>     pipelineAdditiveNoDepthDoubleSided;
    std::unique_ptr<VulkanPipeline>     pipelineLit;
    std::unique_ptr<VulkanPipeline>     pipelineLitDoubleSided;
    std::unique_ptr<VulkanPipeline>     pipelineLitAlphaNoDepth;
    std::unique_ptr<VulkanPipeline>     pipelineLitAlphaNoDepthDoubleSided;
    std::unique_ptr<VulkanPipeline>     pipelineLitAdditiveNoDepth;
    std::unique_ptr<VulkanPipeline>     pipelineLitAdditiveNoDepthDoubleSided;
    std::unique_ptr<FramebufferManager> framebuffers;
    RenderResourceManager*              resources = nullptr;
    ThreadPool*                         threadPool = nullptr;
    VulkanBackendContext&               backendContext;
    DescriptorSetManager&               descriptorSetManager;
    VkExtent2D                          renderExtent{1, 1};
    GtsResourceHandle                   outputHandle;
    GtsResourceHandle                   depthHandle;
    VkImageLayout                       colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    DataSource                          dataSource = DataSource::RuntimeWorld;

    std::array<VkBuffer,       GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBuffers{};
    std::array<VkDeviceMemory, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBufferMemory{};
    std::array<void*,          GraphicsConstants::MAX_FRAMES_IN_FLIGHT> instanceBufferMapped{};

    uint32_t                            lastTriangleCount = 0;
    uint32_t                            lastDrawCalls = 0;
    uint32_t                            lastPipelineBinds = 0;
    uint32_t                            lastDescriptorBinds = 0;
    uint32_t                            lastPipelineSwitches = 0;
    uint32_t                            lastTextureSwitches = 0;

    uint32_t                            maxRecordingSlots = 1;
    std::vector<std::vector<VkCommandPool>>   secondaryCommandPools;
    std::vector<std::vector<VkCommandBuffer>> secondaryCommandBuffers;
    std::vector<PreparedBatch>          preparedBatches;
    std::vector<BatchRange>             chunkRanges;
    std::vector<VkCommandBuffer>        secondaryExecutionBuffers;
    std::vector<ChunkStats>             chunkStats;
    bool                                litFallbackWarningLogged = false;
    bool                                normalMapFallbackWarningLogged = false;

    const std::vector<RenderCommand>& resolveRenderList(GtsFrameGraph& graph) const
    {
        if (dataSource == DataSource::EditorPreview)
            return graph.getData<EditorPreviewRenderData>().renderList;
        return graph.getData<std::vector<RenderCommand>>();
    }

    const MaterialFrameData& resolveMaterialFrameData(GtsFrameGraph& graph) const
    {
        if (dataSource == DataSource::EditorPreview)
            return graph.getData<EditorPreviewRenderData>().materialFrameData;
        return graph.getData<MaterialFrameData>();
    }

    RenderViewportRect resolveViewport(GtsFrameGraph& graph) const
    {
        if (dataSource == DataSource::EditorPreview)
        {
            return graph.getData<EditorPreviewRenderData>().viewport
                .clampedTo(static_cast<int>(renderExtent.width), static_cast<int>(renderExtent.height));
        }
        return graph.getData<RenderViewportFrame>().sceneRenderViewport
            .clampedTo(static_cast<int>(renderExtent.width), static_cast<int>(renderExtent.height));
    }

    void initializeParallelRecordingResources()
    {
        maxRecordingSlots = (threadPool ? threadPool->threadCount() : 0u) + 1u;
        secondaryCommandPools.assign(
            GraphicsConstants::MAX_FRAMES_IN_FLIGHT,
            std::vector<VkCommandPool>(maxRecordingSlots, VK_NULL_HANDLE));
        secondaryCommandBuffers.assign(
            GraphicsConstants::MAX_FRAMES_IN_FLIGHT,
            std::vector<VkCommandBuffer>(maxRecordingSlots, VK_NULL_HANDLE));

        QueueFamilyIndices indices = backendContext.queueFamilyIndices();
        for (uint32_t frameIndex = 0; frameIndex < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            for (uint32_t slot = 0; slot < maxRecordingSlots; ++slot)
            {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

                if (vkCreateCommandPool(backendContext.device(), &poolInfo, nullptr,
                                        &secondaryCommandPools[frameIndex][slot]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create secondary command pool");
                }

                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = secondaryCommandPools[frameIndex][slot];
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocInfo.commandBufferCount = 1;

                if (vkAllocateCommandBuffers(backendContext.device(), &allocInfo,
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
                    vkDestroyCommandPool(backendContext.device(),
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
        lastPipelineBinds = 0;
        lastDescriptorBinds = 0;
        lastPipelineSwitches = 0;
        lastTextureSwitches = 0;
    }

    void resetSecondaryCommandPools(uint32_t currentFrame)
    {
        for (uint32_t slot = 0; slot < maxRecordingSlots; ++slot)
        {
            vkResetCommandPool(backendContext.device(),
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
        renderPassInfo.renderArea.extent = renderExtent;

        clearValues[0].color = dataSource == DataSource::EditorPreview
            ? VkClearColorValue{{0.010f, 0.014f, 0.020f, 1.0f}}
            : VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        return renderPassInfo;
    }

    void setViewportAndScissor(VkCommandBuffer cmd, const RenderViewportRect& viewportRect) const
    {
        VkViewport viewport{};
        viewport.x = static_cast<float>(viewportRect.x);
        viewport.y = static_cast<float>(viewportRect.y);
        viewport.width = static_cast<float>(viewportRect.width);
        viewport.height = static_cast<float>(viewportRect.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {viewportRect.x, viewportRect.y};
        scissor.extent = {
            static_cast<uint32_t>(viewportRect.width),
            static_cast<uint32_t>(viewportRect.height)
        };
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

    void prepareBatches(const std::vector<RenderCommand>& renderList,
                        const MaterialFrameData& materialFrameData,
                        uint32_t currentFrame)
    {
        preparedBatches.clear();

        if (renderList.empty())
            return;

        auto* instanceData = static_cast<uint32_t*>(instanceBufferMapped[currentFrame]);
        uint32_t instanceHead = 0;
        for (const RenderCommand& command : renderList)
        {
            MeshResource* mesh = resources->getMesh(command.meshID);
            if (mesh == nullptr)
                continue;

            auto appendBatch = [&](uint32_t firstIndex,
                                   uint32_t indexCount,
                                   const SubmeshMaterialRuntimeBinding* submeshMaterial)
            {
                if (indexCount == 0)
                    return;

                MaterialGpuHandle materialGpu = command.materialGpu;
                MaterialVariantKey variantKey = command.variantKey;
                RenderQueue renderQueue = command.renderQueue;
                if (submeshMaterial != nullptr && submeshMaterial->valid())
                {
                    materialGpu = submeshMaterial->materialGpu;
                    variantKey = submeshMaterial->variantKey;
                    renderQueue = submeshMaterial->renderQueue;
                }

                const MaterialFrameState* material = materialFrameData.find(materialGpu);
                if (material == nullptr)
                    return;

                if (!preparedBatches.empty())
                {
                    PreparedBatch& previous = preparedBatches.back();
                    if (previous.meshID == command.meshID &&
                        previous.materialGpu == materialGpu &&
                        previous.variantKey == variantKey &&
                        previous.renderQueue == renderQueue &&
                        previous.firstIndex == firstIndex &&
                        previous.indexCount == indexCount)
                    {
                        instanceData[instanceHead] = command.objectSSBOSlot;
                        instanceHead += 1;
                        previous.instanceCount += 1;
                        return;
                    }
                }

                PreparedBatch batch{};
                batch.meshID = command.meshID;
                batch.materialGpu = materialGpu;
                batch.variantKey = variantKey;
                batch.renderQueue = renderQueue;
                batch.material = *material;
                batch.firstIndex = firstIndex;
                batch.indexCount = indexCount;
                batch.instanceOffset = instanceHead;
                batch.instanceCount = 1;
                batch.mesh = mesh;
                batch.materialTextureSets =
                    resources->getMaterialTextureDescriptorSets(material->textures);
                batch.litCompatible = litMaterialCompatible(batch);
                batch.normalMapCompatible = normalMappedMaterialCompatible(batch);
                batch.effectiveFeatureFlags = material->featureFlags;

                if (!batch.litCompatible && !litFallbackWarningLogged)
                {
                    std::cerr
                        << "[rendering] StandardSurface material requires normals; "
                        << "falling back to unlit shading for unsupported mesh "
                        << command.meshID << ".\n";
                    litFallbackWarningLogged = true;
                }
                if (!batch.normalMapCompatible)
                {
                    batch.effectiveFeatureFlags = withoutMaterialFeature(
                        batch.effectiveFeatureFlags,
                        MaterialFeatureFlags::HasNormalTexture);
                    if (!normalMapFallbackWarningLogged)
                    {
                        std::cerr
                            << "[rendering] StandardSurface normal map requires normals, tangents, and UVs; "
                            << "disabling normal mapping for unsupported mesh "
                            << command.meshID << ".\n";
                        normalMapFallbackWarningLogged = true;
                    }
                }

                instanceData[instanceHead] = command.objectSSBOSlot;
                instanceHead += 1;
                preparedBatches.push_back(batch);
            };

            if (command.indexCount != 0)
            {
                appendBatch(command.firstIndex, command.indexCount, nullptr);
            }
            else if (!mesh->submeshes.empty())
            {
                for (size_t submeshIndex = 0; submeshIndex < mesh->submeshes.size(); ++submeshIndex)
                {
                    const gts::rendering::SubmeshAssetData& submesh = mesh->submeshes[submeshIndex];
                    const SubmeshMaterialRuntimeBinding* submeshMaterial =
                        submeshIndex < command.submeshMaterials.size()
                            ? &command.submeshMaterials[submeshIndex]
                            : nullptr;
                    appendBatch(submesh.firstIndex, submesh.indexCount, submeshMaterial);
                }
            }
            else
            {
                appendBatch(0, static_cast<uint32_t>(mesh->indices.size()), nullptr);
            }
        }
    }

    VulkanPipeline* selectPipeline(const PreparedBatch& batch) const
    {
        const bool lit = shouldShadeLit(batch);

        if (batch.material.renderState.blendMode == MaterialBlendMode::Additive)
        {
            if (lit)
            {
                return batch.material.renderState.doubleSided
                    ? pipelineLitAdditiveNoDepthDoubleSided.get()
                    : pipelineLitAdditiveNoDepth.get();
            }

            return batch.material.renderState.doubleSided
                ? pipelineAdditiveNoDepthDoubleSided.get()
                : pipelineAdditiveNoDepth.get();
        }

        if (!batch.material.renderState.depthWrite)
        {
            if (lit)
            {
                return batch.material.renderState.doubleSided
                    ? pipelineLitAlphaNoDepthDoubleSided.get()
                    : pipelineLitAlphaNoDepth.get();
            }

            return batch.material.renderState.doubleSided
                ? pipelineAlphaNoDepthDoubleSided.get()
                : pipelineAlphaNoDepth.get();
        }

        if (lit)
        {
            return batch.material.renderState.doubleSided
                ? pipelineLitDoubleSided.get()
                : pipelineLit.get();
        }

        return batch.material.renderState.doubleSided
            ? pipelineDoubleSided.get()
            : pipeline.get();
    }

    static bool shouldShadeLit(const PreparedBatch& batch)
    {
        return batch.material.shaderFamily == MaterialShaderFamily::StandardSurface
            && !batch.material.vertexColorOnly
            && batch.litCompatible;
    }

    static bool litMaterialCompatible(const PreparedBatch& batch)
    {
        if (batch.material.shaderFamily != MaterialShaderFamily::StandardSurface)
            return true;

        return batch.mesh != nullptr
            && hasVertexAttribute(batch.mesh->metadata.attributes, VertexAttributeFlags::Normal);
    }

    static bool normalMappedMaterialCompatible(const PreparedBatch& batch)
    {
        if (!hasMaterialFeature(batch.material.featureFlags, MaterialFeatureFlags::HasNormalTexture))
            return true;

        if (batch.material.shaderFamily != MaterialShaderFamily::StandardSurface)
            return true;

        return batch.mesh != nullptr
            && hasVertexAttribute(batch.mesh->metadata.attributes, VertexAttributeFlags::Normal)
            && hasVertexAttribute(batch.mesh->metadata.attributes, VertexAttributeFlags::Tangent)
            && hasVertexAttribute(batch.mesh->metadata.attributes, VertexAttributeFlags::UV0);
    }

    static ScenePushConstants makePushConstants(const PreparedBatch& batch)
    {
        ScenePushConstants pushConstants;
        pushConstants.materialFlags = {
            batch.material.vertexColorOnly ? 1 : 0,
            static_cast<int32_t>(batch.effectiveFeatureFlags),
            0,
            0
        };
        pushConstants.baseColor = batch.material.parameters.baseColor;
        pushConstants.surfaceFactors = batch.material.parameters.surfaceParameters;
        pushConstants.emissiveFactorStrength =
            batch.material.parameters.emissiveFactorStrength;
        return pushConstants;
    }

    static bool pushConstantsEqual(const ScenePushConstants& lhs,
                                   const ScenePushConstants& rhs)
    {
        return lhs.materialFlags.x == rhs.materialFlags.x
            && lhs.materialFlags.y == rhs.materialFlags.y
            && lhs.materialFlags.z == rhs.materialFlags.z
            && lhs.materialFlags.w == rhs.materialFlags.w
            && lhs.baseColor == rhs.baseColor
            && lhs.surfaceFactors == rhs.surfaceFactors
            && lhs.emissiveFactorStrength == rhs.emissiveFactorStrength;
    }

    GlobalDescriptorSets resolveGlobalDescriptorSets(uint32_t currentFrame, view_id_type cameraViewID) const
    {
        CameraBufferResource* cameraView = resources->getCameraView(cameraViewID);
        const std::vector<VkDescriptorSet>* environmentSets =
            resources->getEnvironmentTextureDescriptorSets(cameraView->environment);
        return {
            cameraView->descriptorSets[currentFrame],
            resources->getObjectSSBODescriptorSet(currentFrame),
            environmentSets != nullptr ? (*environmentSets)[currentFrame] : VK_NULL_HANDLE
        };
    }

    void bindGlobalDescriptorSets(VkCommandBuffer cmd,
                                  const GlobalDescriptorSets& globalSets,
                                  ChunkStats& stats) const
    {
        VkDescriptorSet sets[2] = {globalSets.camera, globalSets.object};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(),
                                0, 2, sets, 0, nullptr);
        stats.descriptorBinds += 1;

        if (globalSets.environment != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(),
                                    3, 1, &globalSets.environment, 0, nullptr);
            stats.descriptorBinds += 1;
        }
    }

    void recordBatchRange(VkCommandBuffer cmd,
                          uint32_t currentFrame,
                          uint32_t batchStart,
                          uint32_t batchEnd,
                          ChunkStats& stats) const
    {
        const VkDeviceSize zeroOffset = 0;
        VulkanPipeline* boundPipeline = nullptr;
        mesh_id_type    boundMesh     = static_cast<mesh_id_type>(-1);
        MaterialTextureIds boundTextures{};
        bool            hasBoundTextureSet = false;
        ScenePushConstants boundPushConstants{};
        bool            hasBoundPushConstants = false;

        for (uint32_t batchIndex = batchStart; batchIndex < batchEnd; ++batchIndex)
        {
            const PreparedBatch& batch = preparedBatches[batchIndex];
            MeshResource* mesh = batch.mesh;
            if (mesh == nullptr || batch.materialTextureSets == nullptr)
                continue;

            VulkanPipeline* activePipeline = selectPipeline(batch);

            if (activePipeline != boundPipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline->getPipeline());
                boundPipeline = activePipeline;
                stats.pipelineBinds += 1;
                stats.pipelineSwitches += 1;
            }

            if (batch.meshID != boundMesh)
            {
                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, &zeroOffset);
                vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                boundMesh = batch.meshID;
            }

            if (!hasBoundTextureSet || batch.material.textures != boundTextures)
            {
                const VkDescriptorSet materialTextureSet =
                    (*batch.materialTextureSets)[currentFrame];
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        activePipeline->getPipelineLayout(),
                                        2, 1, &materialTextureSet,
                                        0, nullptr);
                boundTextures = batch.material.textures;
                hasBoundTextureSet = true;
                stats.descriptorBinds += 1;
                stats.textureSwitches += 1;
            }

            const ScenePushConstants pushConstants = makePushConstants(batch);
            if (!hasBoundPushConstants || !pushConstantsEqual(pushConstants, boundPushConstants))
            {
                vkCmdPushConstants(cmd, activePipeline->getPipelineLayout(),
                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(ScenePushConstants), &pushConstants);
                boundPushConstants = pushConstants;
                hasBoundPushConstants = true;
            }

            VkDeviceSize instanceOffset = static_cast<VkDeviceSize>(batch.instanceOffset) * sizeof(uint32_t);
            vkCmdBindVertexBuffers(cmd, 1, 1, &instanceBuffers[currentFrame], &instanceOffset);

            vkCmdDrawIndexed(cmd, batch.indexCount, batch.instanceCount, batch.firstIndex, 0, 0);

            stats.triangles += (batch.indexCount / 3) * batch.instanceCount;
            stats.drawCalls += 1;
        }
    }

    void aggregateChunkStats(uint32_t chunkCount)
    {
        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            lastTriangleCount += chunkStats[i].triangles;
            lastDrawCalls += chunkStats[i].drawCalls;
            lastPipelineBinds += chunkStats[i].pipelineBinds;
            lastDescriptorBinds += chunkStats[i].descriptorBinds;
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
        uint32_t remainingCommands = 0;
        for (const PreparedBatch& batch : preparedBatches)
            remainingCommands += batch.instanceCount;
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
                      const std::vector<RenderCommand>& renderList,
                      const RenderViewportRect& viewport)
    {
        std::array<VkClearValue, 2> clearValues{};
        VkRenderPassBeginInfo renderPassInfo = makeRenderPassBeginInfo(imageIndex, clearValues);
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        setViewportAndScissor(cmd, viewport);

        if (preparedBatches.empty() || renderList[0].cameraViewID == 0)
        {
            vkCmdEndRenderPass(cmd);
            return;
        }

        const GlobalDescriptorSets globalSets =
            resolveGlobalDescriptorSets(currentFrame, renderList[0].cameraViewID);
        ChunkStats inlineStats{};
        bindGlobalDescriptorSets(cmd, globalSets, inlineStats);
        recordBatchRange(cmd, currentFrame, 0, static_cast<uint32_t>(preparedBatches.size()), inlineStats);
        chunkStats[0] = inlineStats;
        aggregateChunkStats(1);
        vkCmdEndRenderPass(cmd);
    }

    void recordParallel(VkCommandBuffer cmd,
                        uint32_t imageIndex,
                        uint32_t currentFrame,
                        const std::vector<RenderCommand>& renderList,
                        const RenderViewportRect& viewport)
    {
        const uint32_t chunkCount = buildChunkRanges(renderList);
        if (chunkCount == 0)
        {
            recordInline(cmd, imageIndex, currentFrame, renderList, viewport);
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
            setViewportAndScissor(secondary, viewport);
            bindGlobalDescriptorSets(secondary, globalSets, chunkStats[chunkIndex]);
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
