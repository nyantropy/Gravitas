#pragma once

#include "GlmConfig.h"

// Concrete prepared static profile, below the canonical semantic-stream boundary.
struct GtsStaticVertex
{
    glm::vec3 pos = {0.0f, 0.0f, 0.0f};
    glm::vec3 normal = {0.0f, 0.0f, 1.0f};
    glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 texCoord = {0.0f, 0.0f};

    GtsStaticVertex() = default;

    GtsStaticVertex(glm::vec3 position, glm::vec3 vertexColor, glm::vec2 uv)
        : pos(position)
        , color(vertexColor, 1.0f)
        , texCoord(uv)
    {
    }

    GtsStaticVertex(glm::vec3 position, glm::vec4 vertexColor, glm::vec2 uv)
        : pos(position)
        , color(vertexColor)
        , texCoord(uv)
    {
    }

    GtsStaticVertex(glm::vec3 position,
           glm::vec3 vertexNormal,
           glm::vec4 vertexTangent,
           glm::vec4 vertexColor,
           glm::vec2 uv)
        : pos(position)
        , normal(vertexNormal)
        , tangent(vertexTangent)
        , color(vertexColor)
        , texCoord(uv)
    {
    }

    bool operator==(const GtsStaticVertex& other) const
    {
        return pos == other.pos
            && normal == other.normal
            && tangent == other.tangent
            && color == other.color
            && texCoord == other.texCoord;
    }
};
