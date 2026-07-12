#pragma once

#include <cstdint>

#include "GlmConfig.h"

enum class VertexAttributeFlags : uint32_t
{
    None = 0,
    Position = 1u << 0u,
    Normal = 1u << 1u,
    Tangent = 1u << 2u,
    Color = 1u << 3u,
    UV0 = 1u << 4u
};

inline constexpr VertexAttributeFlags operator|(VertexAttributeFlags lhs, VertexAttributeFlags rhs)
{
    return static_cast<VertexAttributeFlags>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline constexpr VertexAttributeFlags operator&(VertexAttributeFlags lhs, VertexAttributeFlags rhs)
{
    return static_cast<VertexAttributeFlags>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline constexpr VertexAttributeFlags& operator|=(VertexAttributeFlags& lhs, VertexAttributeFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline constexpr bool hasVertexAttribute(VertexAttributeFlags flags, VertexAttributeFlags flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0u;
}

inline constexpr VertexAttributeFlags StandardVertexAttributes =
    VertexAttributeFlags::Position
    | VertexAttributeFlags::Normal
    | VertexAttributeFlags::Tangent
    | VertexAttributeFlags::Color
    | VertexAttributeFlags::UV0;

inline constexpr VertexAttributeFlags UnlitVertexAttributes =
    VertexAttributeFlags::Position
    | VertexAttributeFlags::Color
    | VertexAttributeFlags::UV0;

struct MeshGeometryMetadata
{
    VertexAttributeFlags attributes = VertexAttributeFlags::None;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    bool generatedNormals = false;
    bool generatedTangents = false;
};

struct Vertex
{
    glm::vec3 pos = {0.0f, 0.0f, 0.0f};
    glm::vec3 normal = {0.0f, 0.0f, 1.0f};
    glm::vec4 tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 texCoord = {0.0f, 0.0f};

    Vertex() = default;

    Vertex(glm::vec3 position, glm::vec3 vertexColor, glm::vec2 uv)
        : pos(position)
        , color(vertexColor, 1.0f)
        , texCoord(uv)
    {
    }

    Vertex(glm::vec3 position, glm::vec4 vertexColor, glm::vec2 uv)
        : pos(position)
        , color(vertexColor)
        , texCoord(uv)
    {
    }

    Vertex(glm::vec3 position,
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

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos
            && normal == other.normal
            && tangent == other.tangent
            && color == other.color
            && texCoord == other.texCoord;
    }
};
