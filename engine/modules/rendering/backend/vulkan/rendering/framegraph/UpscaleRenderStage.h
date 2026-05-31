#pragma once

#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>

#include "FramebufferManager.hpp"
#include "FramebufferManagerConfig.h"
#include "GraphicsConstants.h"
#include "GtsFrameGraph.h"
#include "GtsRenderStage.h"
#include "VulkanPipeline.hpp"
#include "VulkanPipelineConfig.h"
#include "VulkanRenderPass.hpp"
#include "VulkanRenderPassConfig.h"
#include "dssheet.h"
#include "vcsheet.h"

class UpscaleRenderStage : public GtsRenderStage
{
public:
    UpscaleRenderStage(VkImageView       sourceImageView,
                       VkFormat          outputFormat,
                       GtsResourceHandle sourceHandle,
                       GtsResourceHandle outputHandle,
                       VkImageLayout     outputFinalLayout)
        : GtsRenderStage("UpscaleRenderStage")
        , sourceHandle(sourceHandle)
        , outputHandle(outputHandle)
        , outputFinalLayout(outputFinalLayout)
    {
        VulkanRenderPassConfig rpConfig;
        rpConfig.colorFormat        = outputFormat;
        rpConfig.colorLoadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        rpConfig.colorInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        rpConfig.colorFinalLayout   = outputFinalLayout;
        rpConfig.hasDepthAttachment = false;
        renderPass = std::make_unique<VulkanRenderPass>(rpConfig);

        VulkanPipelineConfig pConfig;
        pConfig.vertexShaderPath = GraphicsConstants::UPSCALE_V_SHADER_PATH;
        pConfig.fragmentShaderPath = GraphicsConstants::UPSCALE_F_SHADER_PATH;
        pConfig.vkRenderPass = renderPass->getRenderPass();
        pConfig.vertexBindings = {};
        pConfig.vertexAttributes = {};
        pConfig.depthTestEnable = false;
        pConfig.depthWriteEnable = false;
        pConfig.cullMode = VK_CULL_MODE_NONE;
        pConfig.blendEnable = false;
        pConfig.pushConstantSize = 0;
        pConfig.descriptorSetLayouts = {dssheet::getDescriptorSetLayouts()[2]};
        pipeline = std::make_unique<VulkanPipeline>(pConfig);

        FramebufferManagerConfig fbConfig;
        fbConfig.vkRenderpass       = renderPass->getRenderPass();
        fbConfig.hasDepthAttachment = false;
        framebuffers = std::make_unique<FramebufferManager>(fbConfig);

        createSampler();
        sourceDescriptorSets = dssheet::getManager().allocateForSampledImage(
            sourceImageView,
            sourceSampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    ~UpscaleRenderStage() override
    {
        if (!sourceDescriptorSets.empty())
        {
            dssheet::getManager().freeSampledImageSets(sourceDescriptorSets);
            sourceDescriptorSets.clear();
        }

        if (sourceSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(vcsheet::getDevice(), sourceSampler, nullptr);
            sourceSampler = VK_NULL_HANDLE;
        }
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.declareRead(this,
                          sourceHandle,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        graph.declareWrite(this,
                           outputHandle,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                           outputFinalLayout);
    }

    void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                uint32_t imageIndex, uint32_t currentFrame) override
    {
        (void)graph;

        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = renderPass->getRenderPass();
        rpInfo.framebuffer = framebuffers->getFramebuffers()[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = vcsheet::getFrameOutputExtent();
        rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(),
                                0,
                                1,
                                &sourceDescriptorSets[currentFrame],
                                0,
                                nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
    }

private:
    std::unique_ptr<VulkanRenderPass>   renderPass;
    std::unique_ptr<VulkanPipeline>     pipeline;
    std::unique_ptr<FramebufferManager> framebuffers;
    std::vector<VkDescriptorSet>        sourceDescriptorSets;
    VkSampler                           sourceSampler = VK_NULL_HANDLE;
    GtsResourceHandle                   sourceHandle;
    GtsResourceHandle                   outputHandle;
    VkImageLayout                       outputFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    void createSampler()
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
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(vcsheet::getDevice(), &samplerInfo, nullptr, &sourceSampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create upscale sampler!");
    }
};
