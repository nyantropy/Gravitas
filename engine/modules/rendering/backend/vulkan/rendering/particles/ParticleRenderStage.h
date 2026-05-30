#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
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
#include "TextureResource.h"
#include "VulkanDynamicBuffer.h"
#include "VulkanPipeline.hpp"
#include "VulkanPipelineConfig.h"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"
#include "dssheet.h"
#include "vcsheet.h"

class ParticleRenderStage : public GtsRenderStage
{
public:
    ParticleRenderStage(RenderResourceManager* resources,
                        VkImageView            depthImageView,
                        VkFormat               depthFormat,
                        GtsResourceHandle      outputHandle,
                        GtsResourceHandle      depthHandle,
                        VkImageLayout          colorInitialLayout,
                        VkImageLayout          colorFinalLayout)
        : GtsRenderStage("ParticleRenderStage")
        , resources(resources)
        , outputHandle(outputHandle)
        , depthHandle(depthHandle)
        , colorInitialLayout(colorInitialLayout)
        , colorFinalLayout(colorFinalLayout)
    {
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat        = vcsheet::getFrameOutputFormat();
        rpConfig.depthFormat        = depthFormat;
        rpConfig.colorLoadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        rpConfig.colorInitialLayout = colorInitialLayout;
        rpConfig.colorFinalLayout   = colorFinalLayout;
        rpConfig.depthLoadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        rpConfig.depthInitialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        rpConfig.hasDepthAttachment = true;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        VulkanPipelineConfig alphaConfig = makePipelineConfig();
        pipelineAlpha = std::make_unique<VulkanPipeline>(alphaConfig);

        VulkanPipelineConfig additiveConfig = alphaConfig;
        additiveConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        additiveConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        additiveConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        additiveConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineAdditive = std::make_unique<VulkanPipeline>(additiveConfig);

        FramebufferManagerConfig fbConfig;
        fbConfig.vkRenderpass        = renderPass->getRenderPass();
        fbConfig.attachmentImageView = depthImageView;
        fbConfig.hasDepthAttachment  = true;
        framebuffers = std::make_unique<FramebufferManager>(fbConfig);
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<ParticleFrameData>(this);

        graph.declareWrite(this, outputHandle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            colorFinalLayout);

        graph.declareRead(this, depthHandle,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                uint32_t imageIndex, uint32_t currentFrame) override
    {
        const ParticleFrameData& frameData = graph.getData<ParticleFrameData>();
        lastParticleCount = static_cast<uint32_t>(frameData.instances.size());
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
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = vcsheet::getFrameOutputExtent();
        rpInfo.clearValueCount   = 0;
        rpInfo.pClearValues      = nullptr;
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    struct ParticleGpuInstance
    {
        glm::vec4 positionSize = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 color        = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 rotationPad  = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    std::unique_ptr<VulkanRenderPass>   renderPass;
    std::unique_ptr<VulkanPipeline>     pipelineAlpha;
    std::unique_ptr<VulkanPipeline>     pipelineAdditive;
    std::unique_ptr<FramebufferManager> framebuffers;
    RenderResourceManager*              resources = nullptr;
    GtsResourceHandle                   outputHandle;
    GtsResourceHandle                   depthHandle;
    VkImageLayout                       colorInitialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkImageLayout                       colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VulkanDynamicBuffer instanceBuffer{
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        1024u * sizeof(ParticleGpuInstance)
    };
    std::vector<ParticleGpuInstance> uploadScratch;
    uint32_t lastParticleCount = 0;
    uint32_t lastDrawCalls = 0;

    VulkanPipelineConfig makePipelineConfig()
    {
        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding   = 0;
        instanceBinding.stride    = sizeof(ParticleGpuInstance);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputAttributeDescription, 3> attrs{};
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
        attrs[2].offset   = offsetof(ParticleGpuInstance, rotationPad);

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
            dssheet::getDescriptorSetLayouts()[0],
            dssheet::getDescriptorSetLayouts()[2],
        };
        return config;
    }

    void uploadInstances(const ParticleFrameData& frameData)
    {
        uploadScratch.clear();
        uploadScratch.reserve(frameData.instances.size());

        for (const ParticleInstance& instance : frameData.instances)
        {
            uploadScratch.push_back({
                {instance.worldPosition, instance.size},
                instance.color,
                {instance.rotation, instance.depth, 0.0f, 0.0f}
            });
        }

        const VkDeviceSize bytes =
            static_cast<VkDeviceSize>(uploadScratch.size()) * sizeof(ParticleGpuInstance);
        instanceBuffer.ensureCapacity(bytes);
        std::memcpy(instanceBuffer.getMapped(), uploadScratch.data(), static_cast<size_t>(bytes));
    }
};

