#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "VulkanVertexDescription.h"

struct VulkanPipelineConfig
{
    // ── existing fields (unchanged) ──────────────────────────────────────
    std::string  vertexShaderPath;
    std::string  fragmentShaderPath;
    VkRenderPass vkRenderPass = VK_NULL_HANDLE;

    // ── vertex input ─────────────────────────────────────────────────────
    // Defaults to the standard Vertex layout used by the main pipeline.
    // Override for custom vertex types (e.g. TextGlyphVertex).
    // Scene pipeline adds a second per-instance binding for objectSSBOSlot.
    std::vector<VkVertexInputBindingDescription>   vertexBindings   = { VulkanVertexDescription::getBindingDescription() };
    std::vector<VkVertexInputAttributeDescription> vertexAttributes = []
    {
        auto a = VulkanVertexDescription::getAttributeDescriptions();
        return std::vector<VkVertexInputAttributeDescription>(a.begin(), a.end());
    }();

    // ── rasterizer ───────────────────────────────────────────────────────
    // Default is back-face culling for closed meshes.
    // Set to VK_CULL_MODE_NONE for double-sided geometry (planes, quads, foliage).
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

    // ── depth / stencil ──────────────────────────────────────────────────
    bool depthTestEnable  = true;
    bool depthWriteEnable = true;

    // ── blending ─────────────────────────────────────────────────────────
    // Default matches the main pipeline (alpha blending enabled).
    bool          blendEnable           = true;
    VkBlendFactor srcColorBlendFactor   = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColorBlendFactor   = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp     colorBlendOp          = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor   = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor   = VK_BLEND_FACTOR_ZERO;
    VkBlendOp     alphaBlendOp          = VK_BLEND_OP_ADD;

    // ── push constants ────────────────────────────────────────────────────
    // Defaults match the legacy scene pipeline: one 4-byte fragment-stage flag.
    // objectIndex is now a per-instance
    // vertex attribute (instanceObjectIndex at location 5).
    // Set pushConstantSize = 0 to omit push constants entirely.
    uint32_t           pushConstantSize   = sizeof(float);
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT;

    // ── descriptor set layouts ────────────────────────────────────────────
    // Empty means VulkanPipeline uses the backend default descriptor layouts.
    // Non-empty overrides completely.
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};
