#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>

#include "CameraBufferResource.h"
#include "FramebufferManager.hpp"
#include "FramebufferManagerConfig.h"
#include "GtsFrameGraph.h"
#include "GtsRenderStage.h"
#include "GraphicsConstants.h"
#include "ParticleFrameData.h"
#include "RenderResourceManager.hpp"
#include "RenderViewport.h"
#include "MeshResource.h"
#include "TextureResource.h"
#include "Vertex.h"
#include "VulkanDynamicBuffer.h"
#include "VulkanPipeline.hpp"
#include "VulkanPipelineConfig.h"
#include "VulkanVertexDescription.h"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"
#include "DescriptorSetManager.hpp"
#include "VulkanBackendContext.h"

class ParticleRenderStage : public GtsRenderStage
{
    struct ParticleGpuInstance
    {
        glm::vec4 positionSize = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 color        = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 uvRect       = {0.0f, 0.0f, 1.0f, 1.0f};
        glm::vec4 rotationPad  = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 shapeData    = {1.0f, 0.0f, 0.0f, 0.0f};
    };

    struct ParticleMeshGpuInstance
    {
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        glm::vec4 color       = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
    };

public:
    ParticleRenderStage(RenderResourceManager* resources,
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
                        VkImageLayout          colorInitialLayout,
                        VkImageLayout          colorFinalLayout)
        : GtsRenderStage("ParticleRenderStage")
        , resources(resources)
        , backendContext(backendContext)
        , descriptorSetManager(descriptorSetManager)
        , renderExtent(renderExtent)
        , outputHandle(outputHandle)
        , depthHandle(depthHandle)
        , colorInitialLayout(colorInitialLayout)
        , colorFinalLayout(colorFinalLayout)
        , instanceBuffer(backendContext,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         1024u * sizeof(ParticleGpuInstance))
        , meshInstanceBuffer(backendContext,
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             1024u * sizeof(ParticleMeshGpuInstance))
    {
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat        = colorFormat;
        rpConfig.depthFormat        = depthFormat;
        rpConfig.colorLoadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        rpConfig.colorInitialLayout = colorInitialLayout;
        rpConfig.colorFinalLayout   = colorFinalLayout;
        rpConfig.depthLoadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        rpConfig.depthInitialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        rpConfig.depthFinalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        rpConfig.depthAttachmentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        rpConfig.hasDepthAttachment = true;
        renderPass = std::make_unique<VulkanRenderPass>(backendContext, rpConfig);

        VulkanPipelineConfig alphaConfig = makePipelineConfig();
        pipelineAlpha = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, alphaConfig);

        VulkanPipelineConfig additiveConfig = alphaConfig;
        additiveConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        additiveConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        additiveConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        additiveConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineAdditive = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, additiveConfig);

        VulkanPipelineConfig meshAlphaConfig = makeMeshPipelineConfig();
        pipelineMeshAlpha = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, meshAlphaConfig);

        VulkanPipelineConfig meshAdditiveConfig = meshAlphaConfig;
        meshAdditiveConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        meshAdditiveConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        meshAdditiveConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        meshAdditiveConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineMeshAdditive = std::make_unique<VulkanPipeline>(backendContext, descriptorSetManager, meshAdditiveConfig);

        FramebufferManagerConfig fbConfig;
        fbConfig.vkRenderpass        = renderPass->getRenderPass();
        fbConfig.colorImageViews     = colorImageViews;
        fbConfig.attachmentImageView = depthImageView;
        fbConfig.width               = renderExtent.width;
        fbConfig.height              = renderExtent.height;
        fbConfig.framebufferCount    = framebufferCount;
        fbConfig.hasDepthAttachment  = true;
        framebuffers = std::make_unique<FramebufferManager>(backendContext, fbConfig);

        createDepthSampler();
        depthDescriptorSets = descriptorSetManager.allocateForSampledImage(
            depthImageView,
            depthSampler,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }

    ~ParticleRenderStage() override
    {
        if (!depthDescriptorSets.empty())
        {
            descriptorSetManager.freeSampledImageSets(depthDescriptorSets);
            depthDescriptorSets.clear();
        }

        if (depthSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(backendContext.device(), depthSampler, nullptr);
            depthSampler = VK_NULL_HANDLE;
        }
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<ParticleFrameData>(this);
        graph.requestData<RenderViewportFrame>(this);

        graph.declareWrite(this, outputHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            colorFinalLayout);

        graph.declareRead(this, depthHandle,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }

    void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                uint32_t imageIndex, uint32_t currentFrame) override
    {
        const ParticleFrameData& frameData = graph.getData<ParticleFrameData>();
        const RenderViewportRect viewportRect = graph.getData<RenderViewportFrame>().sceneRenderViewport
            .clampedTo(static_cast<int>(renderExtent.width), static_cast<int>(renderExtent.height));
        lastParticleCount = static_cast<uint32_t>(frameData.instances.size() + frameData.meshInstances.size());
        lastDrawCalls = 0;

        if (frameData.empty() || frameData.cameraViewID == 0 || resources == nullptr)
            return;

        CameraBufferResource* cameraView = resources->getCameraView(frameData.cameraViewID);
        if (cameraView == nullptr)
            return;

        uploadInstances(frameData);

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = renderPass->getRenderPass();
        rpInfo.framebuffer       = framebuffers->getFramebuffers()[imageIndex];
        rpInfo.renderArea.offset = {viewportRect.x, viewportRect.y};
        rpInfo.renderArea.extent = {
            static_cast<uint32_t>(viewportRect.width),
            static_cast<uint32_t>(viewportRect.height)
        };
        rpInfo.clearValueCount   = 0;
        rpInfo.pClearValues      = nullptr;
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x        = static_cast<float>(viewportRect.x);
        viewport.y        = static_cast<float>(viewportRect.y);
        viewport.width    = static_cast<float>(viewportRect.width);
        viewport.height   = static_cast<float>(viewportRect.height);
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

        if (!frameData.drawCommands.empty())
        {
            const VkDeviceSize offset = 0;
            VkBuffer instanceVkBuffer = instanceBuffer.getBuffer();
            vkCmdBindVertexBuffers(cmd, 0, 1, &instanceVkBuffer, &offset);

            VulkanPipeline* boundPipeline = nullptr;
            texture_id_type boundTexture = 0;

            for (const ParticleDrawCommand& draw : frameData.drawCommands)
            {
                TextureResource* texture = resources->getTexture(draw.textureID);
                if (texture == nullptr)
                    continue;

                VulkanPipeline* pipeline = draw.blendMode == ParticleBlendMode::Additive
                    ? pipelineAdditive.get()
                    : pipelineAlpha.get();

                if (pipeline != boundPipeline)
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline->getPipelineLayout(),
                                            0, 1,
                                            &cameraView->descriptorSets[currentFrame],
                                            0, nullptr);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline->getPipelineLayout(),
                                            2, 1,
                                            &depthDescriptorSets[currentFrame],
                                            0, nullptr);
                    boundPipeline = pipeline;
                    boundTexture = 0;
                }

                if (draw.textureID != boundTexture)
                {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline->getPipelineLayout(),
                                            1, 1,
                                            &texture->descriptorSets[currentFrame],
                                            0, nullptr);
                    boundTexture = draw.textureID;
                }

                vkCmdDraw(cmd, 6, draw.instanceCount, 0, draw.instanceOffset);
                lastDrawCalls += 1;
            }
        }

        if (!frameData.meshDrawCommands.empty())
        {
            VulkanPipeline* boundPipeline = nullptr;
            mesh_id_type boundMesh = 0;
            texture_id_type boundTexture = 0;
            const VkDeviceSize zeroOffset = 0;
            VkBuffer meshInstanceVkBuffer = meshInstanceBuffer.getBuffer();

            for (const ParticleMeshDrawCommand& draw : frameData.meshDrawCommands)
            {
                MeshResource* mesh = resources->getMesh(draw.meshID);
                TextureResource* texture = resources->getTexture(draw.textureID);
                if (mesh == nullptr || texture == nullptr)
                    continue;

                VulkanPipeline* pipeline = draw.blendMode == ParticleBlendMode::Additive
                    ? pipelineMeshAdditive.get()
                    : pipelineMeshAlpha.get();

                if (pipeline != boundPipeline)
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline->getPipelineLayout(),
                                            0, 1,
                                            &cameraView->descriptorSets[currentFrame],
                                            0, nullptr);
                    boundPipeline = pipeline;
                    boundMesh = 0;
                    boundTexture = 0;
                }

                if (draw.meshID != boundMesh)
                {
                    VkBuffer vertexBuffers[2] = {mesh->vertexBuffer, meshInstanceVkBuffer};
                    VkDeviceSize offsets[2] = {
                        0,
                        static_cast<VkDeviceSize>(draw.instanceOffset) * sizeof(ParticleMeshGpuInstance)
                    };
                    vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, offsets);
                    vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, zeroOffset, VK_INDEX_TYPE_UINT32);
                    boundMesh = draw.meshID;
                }
                else
                {
                    const VkDeviceSize instanceOffset =
                        static_cast<VkDeviceSize>(draw.instanceOffset) * sizeof(ParticleMeshGpuInstance);
                    vkCmdBindVertexBuffers(cmd, 1, 1, &meshInstanceVkBuffer, &instanceOffset);
                }

                if (draw.textureID != boundTexture)
                {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline->getPipelineLayout(),
                                            1, 1,
                                            &texture->descriptorSets[currentFrame],
                                            0, nullptr);
                    boundTexture = draw.textureID;
                }

                const uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
                vkCmdDrawIndexed(cmd, indexCount, draw.instanceCount, 0, 0, 0);
                lastDrawCalls += 1;
            }
        }

        vkCmdEndRenderPass(cmd);
    }

    uint32_t getLastParticleCount() const
    {
        return lastParticleCount;
    }

    uint32_t getLastDrawCalls() const
    {
        return lastDrawCalls;
    }

private:
    std::unique_ptr<VulkanRenderPass>   renderPass;
    std::unique_ptr<VulkanPipeline>     pipelineAlpha;
    std::unique_ptr<VulkanPipeline>     pipelineAdditive;
    std::unique_ptr<VulkanPipeline>     pipelineMeshAlpha;
    std::unique_ptr<VulkanPipeline>     pipelineMeshAdditive;
    std::unique_ptr<FramebufferManager> framebuffers;
    std::vector<VkDescriptorSet>        depthDescriptorSets;
    RenderResourceManager*              resources = nullptr;
    VulkanBackendContext&               backendContext;
    DescriptorSetManager&               descriptorSetManager;
    VkExtent2D                          renderExtent{1, 1};
    GtsResourceHandle                   outputHandle;
    GtsResourceHandle                   depthHandle;
    VkImageLayout                       colorInitialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkImageLayout                       colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkSampler                           depthSampler = VK_NULL_HANDLE;

    VulkanDynamicBuffer instanceBuffer;
    VulkanDynamicBuffer meshInstanceBuffer;
    std::vector<ParticleGpuInstance> uploadScratch;
    std::vector<ParticleMeshGpuInstance> meshUploadScratch;
    uint32_t lastParticleCount = 0;
    uint32_t lastDrawCalls = 0;

    VulkanPipelineConfig makePipelineConfig()
    {
        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding   = 0;
        instanceBinding.stride    = sizeof(ParticleGpuInstance);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputAttributeDescription, 5> attrs{};
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[0].offset   = offsetof(ParticleGpuInstance, positionSize);

        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[1].offset   = offsetof(ParticleGpuInstance, color);

        attrs[2].binding  = 0;
        attrs[2].location = 2;
        attrs[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[2].offset   = offsetof(ParticleGpuInstance, uvRect);

        attrs[3].binding  = 0;
        attrs[3].location = 3;
        attrs[3].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[3].offset   = offsetof(ParticleGpuInstance, rotationPad);

        attrs[4].binding  = 0;
        attrs[4].location = 4;
        attrs[4].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[4].offset   = offsetof(ParticleGpuInstance, shapeData);

        VulkanPipelineConfig config;
        config.vertexShaderPath   = GraphicsConstants::PARTICLE_V_SHADER_PATH;
        config.fragmentShaderPath = GraphicsConstants::PARTICLE_F_SHADER_PATH;
        config.vkRenderPass       = renderPass->getRenderPass();
        config.vertexBindings     = {instanceBinding};
        config.vertexAttributes   = std::vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
        config.cullMode           = VK_CULL_MODE_NONE;
        config.depthTestEnable    = true;
        config.depthWriteEnable   = false;
        config.blendEnable        = true;
        config.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        config.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        config.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        config.pushConstantSize   = 0;
        config.descriptorSetLayouts = {
            descriptorSetManager.getDescriptorSetLayouts()[0],
            descriptorSetManager.getDescriptorSetLayouts()[2],
            descriptorSetManager.getDescriptorSetLayouts()[2],
        };
        return config;
    }

    VulkanPipelineConfig makeMeshPipelineConfig()
    {
        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding   = 1;
        instanceBinding.stride    = sizeof(ParticleMeshGpuInstance);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        auto vertexAttrs = VulkanVertexDescription::getAttributeDescriptions();
        std::vector<VkVertexInputAttributeDescription> attrs(vertexAttrs.begin(), vertexAttrs.end());

        for (uint32_t i = 0; i < 4; ++i)
        {
            VkVertexInputAttributeDescription attr{};
            attr.binding = 1;
            attr.location = 3 + i;
            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr.offset = offsetof(ParticleMeshGpuInstance, modelMatrix) + sizeof(glm::vec4) * i;
            attrs.push_back(attr);
        }

        VkVertexInputAttributeDescription colorAttr{};
        colorAttr.binding = 1;
        colorAttr.location = 7;
        colorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        colorAttr.offset = offsetof(ParticleMeshGpuInstance, color);
        attrs.push_back(colorAttr);

        VkVertexInputAttributeDescription uvAttr{};
        uvAttr.binding = 1;
        uvAttr.location = 8;
        uvAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        uvAttr.offset = offsetof(ParticleMeshGpuInstance, uvTransform);
        attrs.push_back(uvAttr);

        VulkanPipelineConfig config;
        config.vertexShaderPath   = GraphicsConstants::PARTICLE_MESH_V_SHADER_PATH;
        config.fragmentShaderPath = GraphicsConstants::PARTICLE_MESH_F_SHADER_PATH;
        config.vkRenderPass       = renderPass->getRenderPass();
        config.vertexBindings     = {VulkanVertexDescription::getBindingDescription(), instanceBinding};
        config.vertexAttributes   = attrs;
        config.cullMode           = VK_CULL_MODE_NONE;
        config.depthTestEnable    = true;
        config.depthWriteEnable   = false;
        config.blendEnable        = true;
        config.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        config.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        config.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        config.pushConstantSize   = 0;
        config.descriptorSetLayouts = {
            descriptorSetManager.getDescriptorSetLayouts()[0],
            descriptorSetManager.getDescriptorSetLayouts()[2],
        };
        return config;
    }

    void createDepthSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(backendContext.device(), &samplerInfo, nullptr, &depthSampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create particle depth sampler!");
    }

    void uploadInstances(const ParticleFrameData& frameData)
    {
        uploadScratch.clear();
        uploadScratch.reserve(frameData.instances.size());

        for (const ParticleInstance& instance : frameData.instances)
        {
            uploadScratch.push_back({
                {instance.worldPosition, instance.size.x},
                instance.color,
                instance.uvRect,
                {instance.rotation, instance.size.y, instance.softness, static_cast<float>(instance.spriteShape)},
                {instance.spriteEdgeSoftness, 0.0f, 0.0f, 0.0f}
            });
        }

        const VkDeviceSize bytes =
            static_cast<VkDeviceSize>(uploadScratch.size()) * sizeof(ParticleGpuInstance);
        instanceBuffer.ensureCapacity(bytes);
        if (bytes > 0)
            std::memcpy(instanceBuffer.getMapped(), uploadScratch.data(), static_cast<size_t>(bytes));

        meshUploadScratch.clear();
        meshUploadScratch.reserve(frameData.meshInstances.size());
        for (const ParticleMeshInstance& instance : frameData.meshInstances)
        {
            meshUploadScratch.push_back({
                instance.modelMatrix,
                instance.color,
                instance.uvTransform
            });
        }

        const VkDeviceSize meshBytes =
            static_cast<VkDeviceSize>(meshUploadScratch.size()) * sizeof(ParticleMeshGpuInstance);
        meshInstanceBuffer.ensureCapacity(meshBytes);
        if (meshBytes > 0)
            std::memcpy(meshInstanceBuffer.getMapped(), meshUploadScratch.data(), static_cast<size_t>(meshBytes));
    }
};
