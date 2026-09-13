#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include <vulkan/vulkan.h>

#include "assets/processing/geometry/skinned/GtsSkinnedVertex.h"

struct VulkanSkinnedVertexDescription
{
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(GtsSkinnedVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 7> getAttributeDescriptions()
    {
        static_assert(std::is_standard_layout_v<GtsSkinnedVertex>);

        std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions{};

        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset   = offsetof(GtsSkinnedVertex, pos);

        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset   = offsetof(GtsSkinnedVertex, normal);

        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[2].offset   = offsetof(GtsSkinnedVertex, tangent);

        attributeDescriptions[3].binding  = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[3].offset   = offsetof(GtsSkinnedVertex, color);

        attributeDescriptions[4].binding  = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[4].offset   = offsetof(GtsSkinnedVertex, texCoord);

        attributeDescriptions[5] = {5, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(GtsSkinnedVertex, joints)};
        attributeDescriptions[6] = {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GtsSkinnedVertex, weights)};

        return attributeDescriptions;
    }
};
