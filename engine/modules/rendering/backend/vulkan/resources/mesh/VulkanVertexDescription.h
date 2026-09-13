#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include <vulkan/vulkan.h>

#include "assets/processing/geometry/static/GtsStaticVertex.h"

struct VulkanVertexDescription
{
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(GtsStaticVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions()
    {
        static_assert(std::is_standard_layout_v<GtsStaticVertex>);

        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset   = offsetof(GtsStaticVertex, pos);

        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset   = offsetof(GtsStaticVertex, normal);

        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[2].offset   = offsetof(GtsStaticVertex, tangent);

        attributeDescriptions[3].binding  = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[3].offset   = offsetof(GtsStaticVertex, color);

        attributeDescriptions[4].binding  = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[4].offset   = offsetof(GtsStaticVertex, texCoord);

        return attributeDescriptions;
    }
};
