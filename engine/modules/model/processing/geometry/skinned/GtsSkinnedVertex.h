#pragma once

#include "GlmConfig.h"

// CPU profile only; no GPU packing/alignment contract is established here
struct GtsSkinnedVertex
{
    glm::vec3 pos = {0.0f, 0.0f, 0.0f};
    glm::vec3 normal = {0.0f, 0.0f, 1.0f};
    glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 texCoord = {0.0f, 0.0f};
    glm::uvec4 joints{0}; // skin-local slots, never skeleton-node indices
    glm::vec4 weights{0};

    bool operator==(const GtsSkinnedVertex& other) const
    {
        return pos == other.pos
            && normal == other.normal
            && tangent == other.tangent
            && color == other.color
            && texCoord == other.texCoord
            && joints == other.joints
            && weights == other.weights;
    }
};
