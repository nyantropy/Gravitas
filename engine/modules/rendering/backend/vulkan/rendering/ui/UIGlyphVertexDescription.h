#pragma once

#include <array>
#include <vulkan/vulkan.h>
#include <glm.hpp>

// Vulkan vertex input descriptions for the UI glyph vertex format.
//   layout(location = 0) in vec2 inPos;  — offset  0, 8 bytes
//   layout(location = 1) in vec2 inUV;   — offset  8, 8 bytes
// Stride: 16 bytes  (matches sizeof(UIGlyphVertex) from UICommand.h).
struct UIGlyphVertexDescription
{
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription desc{};
        desc.binding   = 0;
        desc.stride    = static_cast<uint32_t>(sizeof(glm::vec2) * 2); // 16 bytes
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};

        // inPos: vec2 at offset 0
        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[0].offset   = 0;

        // inUV: vec2 at offset 8
        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset   = static_cast<uint32_t>(sizeof(glm::vec2));

        return attrs;
    }
};
