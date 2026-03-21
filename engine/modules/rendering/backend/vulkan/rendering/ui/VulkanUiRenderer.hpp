#pragma once

#include <vector>
#include <stdexcept>
#include <cstring>

#include <vulkan/vulkan.h>
#include "GlmConfig.h"

#include "vcsheet.h"
#include "dssheet.h"
#include "BufferUtil.hpp"
#include "MemoryUtil.hpp"
#include "VulkanShader.hpp"
#include "RenderResourceManager.hpp"
#include <UICommand.h>         // in engine/modules/rendering/extractors/ via include path
#include "GraphicsConstants.h"
#include "UIGlyphVertexDescription.h"

// Records the UI overlay render pass into an existing command buffer.
// Owned by ForwardRenderer.  Call record() between vkCmdEndRenderPass (main)
// and vkEndCommandBuffer.
//
// UI render pass: loadOp = LOAD, no depth, composites onto the 3D scene.
// UI pipeline:    alpha blending, set 0 = combined image sampler (font atlas).
// Vertex buffer:  host-visible, persistently mapped, updated each frame.
class VulkanUiRenderer
{
public:
    VulkanUiRenderer()
    {
        createRenderPass();
        createPipelineLayout();
        createPipeline();
        createFramebuffers();
        createDynamicBuffers();
    }

    ~VulkanUiRenderer()
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

        for (auto fb : framebuffers)
            if (fb != VK_NULL_HANDLE)
                vkDestroyFramebuffer(dev, fb, nullptr);

        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(dev, pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(dev, pipelineLayout, nullptr);
        if (renderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(dev, renderPass, nullptr);
    }

    // Records the UI pass into cmd for the given swapchain imageIndex.
    // resources is used to resolve textureID → VkDescriptorSet.
    void record(VkCommandBuffer          cmd,
                uint32_t                 imageIndex,
                uint32_t                 currentFrame,
                const std::vector<UICommandList>& uiLists,
                RenderResourceManager*   resources)
    {
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

        printf("UI: lists=%zu totalVerts=%u\n", uiLists.size(), totalVerts);

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
        rpInfo.renderPass        = renderPass;
        rpInfo.framebuffer       = framebuffers[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = vcsheet::getSwapChainExtent();
        rpInfo.clearValueCount   = 0;
        rpInfo.pClearValues      = nullptr;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.x = 0.0f; vp.y = 0.0f;
        vp.width  = static_cast<float>(vcsheet::getSwapChainExtent().width);
        vp.height = static_cast<float>(vcsheet::getSwapChainExtent().height);
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vcsheet::getSwapChainExtent();
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Ortho proj: [0..1] screen-space, Y-down for Vulkan.
        // glm::ortho(left, right, bottom, top) — then Vulkan Y-flip.
        glm::mat4 proj = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
        //proj[1][1] *= -1.0f;
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &proj);

        const VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // One draw call per atlas.
        uint32_t indexBase  = 0;
        for (const auto& list : uiLists)
        {
            TextureResource* tex = resources->getTexture(list.textureID);
            if (!tex) { indexBase += static_cast<uint32_t>(list.indices.size()); continue; }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1,
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
    VkRenderPass   renderPass     = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline     pipeline       = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> framebuffers;

    VkBuffer       vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    void*          vertexMapped = nullptr;
    uint32_t       vertexCap    = 0;

    VkBuffer       indexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory  = VK_NULL_HANDLE;
    void*          indexMapped  = nullptr;
    uint32_t       indexCap     = 0;

    // Minimum initial capacity.
    static constexpr uint32_t INITIAL_VERTEX_CAP = 4096;
    static constexpr uint32_t INITIAL_INDEX_CAP  = 8192;

    void createRenderPass()
    {
        // Color-only pass: load existing 3D scene, composite UI on top.
        VkAttachmentDescription colorAtt{};
        colorAtt.format         = vcsheet::getSwapChainImageFormat();
        colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAtt.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments    = &colorAtt;
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;

        if (vkCreateRenderPass(vcsheet::getDevice(), &rpInfo, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("VulkanUiRenderer: failed to create UI render pass");
    }

    void createPipelineLayout()
    {
        // set 0 = combined image sampler (reuse existing layout from dssheet[2])
        VkDescriptorSetLayout samplerLayout = dssheet::getDescriptorSetLayouts()[2];

        // push constant: mat4 (64 bytes) — ortho projection, vertex stage only
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pc.offset     = 0;
        pc.size       = sizeof(glm::mat4);

        VkPipelineLayoutCreateInfo info{};
        info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount         = 1;
        info.pSetLayouts            = &samplerLayout;
        info.pushConstantRangeCount = 1;
        info.pPushConstantRanges    = &pc;

        if (vkCreatePipelineLayout(vcsheet::getDevice(), &info, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("VulkanUiRenderer: failed to create pipeline layout");
    }

    void createPipeline()
    {
        VulkanShaderConfig vsCfg;
        vsCfg.shaderFile = GraphicsConstants::UI_V_SHADER_PATH;
        vsCfg.vkDevice   = vcsheet::getDevice();
        VulkanShader vertShader(vsCfg);

        VulkanShaderConfig fsCfg;
        fsCfg.shaderFile = GraphicsConstants::UI_F_SHADER_PATH;
        fsCfg.vkDevice   = vcsheet::getDevice();
        VulkanShader fragShader(fsCfg);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertShader.getShaderModule();
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragShader.getShaderModule();
        stages[1].pName  = "main";

        auto binding    = UIGlyphVertexDescription::getBindingDescription();
        auto attributes = UIGlyphVertexDescription::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions    = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Alpha blending for glyph transparency.
        VkPipelineColorBlendAttachmentState blend{};
        blend.blendEnable         = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp        = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend.alphaBlendOp        = VK_BLEND_OP_ADD;
        blend.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blending{};
        blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blending.attachmentCount = 1;
        blending.pAttachments    = &blend;

        // No depth test — UI always draws on top.
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;

        std::vector<VkDynamicState> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynState{};
        dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynState.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
        dynState.pDynamicStates    = dynStates.data();

        VkGraphicsPipelineCreateInfo pipeInfo{};
        pipeInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeInfo.stageCount          = 2;
        pipeInfo.pStages             = stages;
        pipeInfo.pVertexInputState   = &vertexInput;
        pipeInfo.pInputAssemblyState = &assembly;
        pipeInfo.pViewportState      = &viewportState;
        pipeInfo.pRasterizationState = &raster;
        pipeInfo.pMultisampleState   = &ms;
        pipeInfo.pDepthStencilState  = &depthStencil;
        pipeInfo.pColorBlendState    = &blending;
        pipeInfo.pDynamicState       = &dynState;
        pipeInfo.layout              = pipelineLayout;
        pipeInfo.renderPass          = renderPass;
        pipeInfo.subpass             = 0;

        if (vkCreateGraphicsPipelines(vcsheet::getDevice(), VK_NULL_HANDLE, 1, &pipeInfo,
                                      nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("VulkanUiRenderer: failed to create UI pipeline");
    }

    void createFramebuffers()
    {
        const auto& imageViews = vcsheet::getSwapChainImageViews();
        const auto  extent     = vcsheet::getSwapChainExtent();

        framebuffers.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = renderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = &imageViews[i];
            fbInfo.width           = extent.width;
            fbInfo.height          = extent.height;
            fbInfo.layers          = 1;

            if (vkCreateFramebuffer(vcsheet::getDevice(), &fbInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("VulkanUiRenderer: failed to create framebuffer");
        }
    }

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
