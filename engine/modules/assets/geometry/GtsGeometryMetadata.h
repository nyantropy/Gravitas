#pragma once

#include <cstdint>

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
