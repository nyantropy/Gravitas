#pragma once

#include <array>
#include <vulkan/vulkan.h>
#include "GlmConfig.h"
#include <UiCommand.h>

// Vulkan vertex input descriptions for the UI vertex format.
//   layout(location = 0) in vec2 inPos;   — offset  0, 8 bytes
//   layout(location = 1) in vec2 inUV;    — offset  8, 8 bytes
//   layout(location = 2) in vec4 inColor; — offset 16, 16 bytes
// Stride: 32 bytes  (matches sizeof(UiVertex)).
struct UiVertexDescription
{
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription desc{};
        desc.binding   = 0;
        desc.stride    = sizeof(UiVertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attrs{};

        // inPos: vec2 at offset 0
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[0].offset   = offsetof(UiVertex, pos);

        // inUV: vec2 at offset 8
        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset   = offsetof(UiVertex, uv);

        // inColor: vec4 at offset 16
        attrs[2].binding  = 0;
        attrs[2].location = 2;
        attrs[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[2].offset   = offsetof(UiVertex, color);

        return attrs;
    }
};
