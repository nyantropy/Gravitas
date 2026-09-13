#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "VulkanStaticVertexDescription.h"

struct VulkanPipelineConfig
{
    // ── existing fields (unchanged) ──────────────────────────────────────
    std::string  vertexShaderPath;
    std::string  fragmentShaderPath;
    VkRenderPass vkRenderPass = VK_NULL_HANDLE;

    // ── vertex input ─────────────────────────────────────────────────────
    // defaults to the standard GtsStaticVertex layout used by the main pipeline
    // override for custom vertex types (e.g. TextGlyphVertex)
    // scene pipeline adds a second per-instance binding for objectSSBOSlot
    std::vector<VkVertexInputBindingDescription>   vertexBindings   = { VulkanStaticVertexDescription::getBindingDescription() };
    std::vector<VkVertexInputAttributeDescription> vertexAttributes = []
    {
        auto a = VulkanStaticVertexDescription::getAttributeDescriptions();
        return std::vector<VkVertexInputAttributeDescription>(a.begin(), a.end());
    }();

    // ── rasterizer ───────────────────────────────────────────────────────
    // default is back-face culling for closed meshes
    // set to VK_CULL_MODE_NONE for double-sided geometry (planes, quads, foliage)
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

    // ── depth / stencil ──────────────────────────────────────────────────
    bool depthTestEnable  = true;
    bool depthWriteEnable = true;

    // ── blending ─────────────────────────────────────────────────────────
    // default matches the main pipeline (alpha blending enabled)
    bool          blendEnable           = true;
    VkBlendFactor srcColorBlendFactor   = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColorBlendFactor   = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp     colorBlendOp          = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor   = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor   = VK_BLEND_FACTOR_ZERO;
    VkBlendOp     alphaBlendOp          = VK_BLEND_OP_ADD;

    // ── push constants ────────────────────────────────────────────────────
    // defaults match the current scene pipeline: one 4-byte fragment-stage flag
    // objectIndex is now a per-instance
    // vertex attribute (instanceObjectIndex at location 5)
    // Set pushConstantSize = 0 to omit push constants entirely
    uint32_t           pushConstantSize   = sizeof(float);
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT;

    // ── descriptor set layouts ────────────────────────────────────────────
    // empty means VulkanPipeline uses the backend default descriptor layouts
    // non-empty overrides completely
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};
