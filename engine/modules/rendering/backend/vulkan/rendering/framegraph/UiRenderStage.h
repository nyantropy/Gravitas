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
#include "BufferUtil.hpp"
#include "MemoryUtil.hpp"
#include "dssheet.h"
#include "vcsheet.h"
#include <UICommand.h>

// UI overlay render stage.
// Owns its render pass, pipeline, framebuffers, and dynamic vertex/index buffers.
// Composites screen-space text over the 3D scene.
// Reads the swapchain in PRESENT_SRC_KHR (left by SceneRenderStage) and
// writes it back to PRESENT_SRC_KHR — same layout, so no barrier is inserted.
class UiRenderStage : public GtsRenderStage
{
public:
    UiRenderStage(RenderResourceManager* resources,
                  GtsResourceHandle      swapchainHandle)
        : GtsRenderStage("UiRenderStage")
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

        createDynamicBuffers();
    }

    ~UiRenderStage()
    {
        VkDevice dev = vcsheet::getDevice();

        if (vertexBuffer != VK_NULL_HANDLE)
        {
            vkUnmapMemory(dev, vertexMemory);
            vkDestroyBuffer(dev, vertexBuffer, nullptr);
            vkFreeMemory(dev, vertexMemory, nullptr);
        }
        if (indexBuffer != VK_NULL_HANDLE)
        {
            vkUnmapMemory(dev, indexMemory);
            vkDestroyBuffer(dev, indexBuffer, nullptr);
            vkFreeMemory(dev, indexMemory, nullptr);
        }
    }

    void declareResources(GtsFrameGraph& graph) override
    {
        graph.requestData<std::vector<UICommandList>>(this);

        // Read dependency on the swapchain — creates the scene→UI ordering edge.
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
        const auto& uiLists = graph.getData<std::vector<UICommandList>>();

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

        ensureBufferCapacity(totalVerts, totalIndices);

        auto* vDst = static_cast<UIGlyphVertex*>(vertexMapped);
        auto* iDst = static_cast<uint32_t*>(indexMapped);
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

        // Begin UI render pass.
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
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

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

private:
    std::unique_ptr<VulkanRenderPass>     renderPass;
    std::unique_ptr<VulkanPipeline>       pipeline;
    std::unique_ptr<VulkanFramebufferSet> framebuffers;
    RenderResourceManager*                resources;
    GtsResourceHandle                     swapchainHandle;

    // Dynamic buffers — host-visible, persistently mapped, resized on demand.
    VkBuffer       vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    void*          vertexMapped = nullptr;
    uint32_t       vertexCap    = 0;

    VkBuffer       indexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory  = VK_NULL_HANDLE;
    void*          indexMapped  = nullptr;
    uint32_t       indexCap     = 0;

    static constexpr uint32_t INITIAL_VERTEX_CAP = 4096;
    static constexpr uint32_t INITIAL_INDEX_CAP  = 8192;

    void createDynamicBuffers()
    {
        allocateHostBuffer(
            sizeof(UIGlyphVertex) * INITIAL_VERTEX_CAP,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertexBuffer, vertexMemory, vertexMapped);
        vertexCap = INITIAL_VERTEX_CAP;

        allocateHostBuffer(
            sizeof(uint32_t) * INITIAL_INDEX_CAP,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indexBuffer, indexMemory, indexMapped);
        indexCap = INITIAL_INDEX_CAP;
    }

    void ensureBufferCapacity(uint32_t neededVerts, uint32_t neededIndices)
    {
        VkDevice dev = vcsheet::getDevice();

        if (neededVerts > vertexCap)
        {
            vkUnmapMemory(dev, vertexMemory);
            vkDestroyBuffer(dev, vertexBuffer, nullptr);
            vkFreeMemory(dev, vertexMemory, nullptr);

            vertexCap = neededVerts * 2;
            allocateHostBuffer(sizeof(UIGlyphVertex) * vertexCap,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               vertexBuffer, vertexMemory, vertexMapped);
        }

        if (neededIndices > indexCap)
        {
            vkUnmapMemory(dev, indexMemory);
            vkDestroyBuffer(dev, indexBuffer, nullptr);
            vkFreeMemory(dev, indexMemory, nullptr);

            indexCap = neededIndices * 2;
            allocateHostBuffer(sizeof(uint32_t) * indexCap,
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                               indexBuffer, indexMemory, indexMapped);
        }
    }

    void allocateHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkBuffer& buf, VkDeviceMemory& mem, void*& mapped)
    {
        BufferUtil::createBuffer(
            vcsheet::getDevice(),
            vcsheet::getPhysicalDevice(),
            size, usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buf, mem);

        vkMapMemory(vcsheet::getDevice(), mem, 0, size, 0, &mapped);
    }
};
