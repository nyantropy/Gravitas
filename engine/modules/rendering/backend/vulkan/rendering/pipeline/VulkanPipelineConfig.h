#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "Vertex.h"

struct VulkanPipelineConfig
{
    // ── existing fields (unchanged) ──────────────────────────────────────
    std::string  vertexShaderPath;
    std::string  fragmentShaderPath;
    VkRenderPass vkRenderPass = VK_NULL_HANDLE;

    // ── vertex input ─────────────────────────────────────────────────────
    // Defaults to the standard Vertex layout used by the main pipeline.
    // Override for custom vertex types (e.g. TextGlyphVertex).
    VkVertexInputBindingDescription                vertexBinding    = Vertex::getBindingDescription();
    std::vector<VkVertexInputAttributeDescription> vertexAttributes = []
    {
        auto a = Vertex::getAttributeDescriptions();
        return std::vector<VkVertexInputAttributeDescription>(a.begin(), a.end());
    }();

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
    // Defaults match the main pipeline: objectIndex (uint32) + alpha (float)
    // = 8 bytes, visible to vertex and fragment stages.
    // Set pushConstantSize = 0 to omit push constants entirely.
    uint32_t           pushConstantSize   = sizeof(uint32_t) + sizeof(float);
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // ── descriptor set layouts ────────────────────────────────────────────
    // Empty means VulkanPipeline uses the global dssheet layouts (current
    // main pipeline behavior). Non-empty overrides completely.
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};
