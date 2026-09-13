#pragma once

#include <array>
#include <stdexcept>
#include <utility>
#include "VulkanPipelineConfig.h"
#include "VulkanSkinnedVertexDescription.h"

// Retains the caller's scene fragment/material/raster/depth configuration.
inline VulkanPipelineConfig makeVulkanSkinnedPipelineConfig(VulkanPipelineConfig                        sceneConfig,
                                                            const std::array<VkDescriptorSetLayout, 4>& sceneLayouts,
                                                            VkDescriptorSetLayout                       paletteLayout,
                                                            std::string skinnedVertexShaderPath)
{
    if (!paletteLayout || skinnedVertexShaderPath.empty())
        throw std::invalid_argument("Skinned pipeline requires a palette layout and vertex shader");
    for (auto layout : sceneLayouts)
        if (!layout)
            throw std::invalid_argument("Skinned pipeline requires all four scene descriptor layouts");
    sceneConfig.vertexShaderPath = std::move(skinnedVertexShaderPath);
    sceneConfig.vertexBindings   = {VulkanSkinnedVertexDescription::getBindingDescription(),
                                    {1, sizeof(uint32_t), VK_VERTEX_INPUT_RATE_INSTANCE}};
    const auto attributes        = VulkanSkinnedVertexDescription::getAttributeDescriptions();
    sceneConfig.vertexAttributes.assign(attributes.begin(), attributes.end());
    sceneConfig.vertexAttributes.push_back({7, 1, VK_FORMAT_R32_UINT, 0});
    sceneConfig.descriptorSetLayouts.assign(sceneLayouts.begin(), sceneLayouts.end());
    sceneConfig.descriptorSetLayouts.push_back(paletteLayout);
    return sceneConfig;
}
