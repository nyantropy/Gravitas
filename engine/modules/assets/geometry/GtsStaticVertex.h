#pragma once

#include "GlmConfig.h"

// concrete prepared static profile, below the canonical semantic-stream boundary
struct GtsStaticVertex
{
    glm::vec3 pos = {0.0f, 0.0f, 0.0f};
    glm::vec3 normal = {0.0f, 0.0f, 1.0f};
    glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 texCoord = {0.0f, 0.0f};

    bool operator==(const GtsStaticVertex& other) const
    {
        return pos == other.pos
            && normal == other.normal
            && tangent == other.tangent
            && color == other.color
            && texCoord == other.texCoord;
    }
};
