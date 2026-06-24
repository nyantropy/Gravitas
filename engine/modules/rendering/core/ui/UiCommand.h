#pragma once

#include <cmath>
#include <vector>

#include "GlmConfig.h"
#include "Types.h"

// Vertex format for UI primitives (colored quads, textured quads).
struct UiVertex
{
    glm::vec2 pos;
    glm::vec2 uv    = {0.0f, 0.0f};
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

enum class UiDrawType
{
    ColoredQuad,
    TexturedQuad
};

struct UiDrawCommand
{
    UiDrawType      type        = UiDrawType::ColoredQuad;
    texture_id_type textureID   = 0;
    uint32_t        indexOffset = 0;
    uint32_t        indexCount  = 0;
};

// All renderer-facing UI draw data for one frame.
struct UiCommandBuffer
{
    std::vector<UiVertex>      vertices;
    std::vector<uint32_t>      indices;
    std::vector<UiDrawCommand> commands;

    bool empty() const { return commands.empty(); }

    void clear()
    {
        vertices.clear();
        indices.clear();
        commands.clear();
    }

    void addDrawCommand(UiDrawType type,
                        texture_id_type textureID,
                        uint32_t indexOffset,
                        uint32_t indexCount)
    {
        if (indexCount == 0)
            return;

        if (!commands.empty())
        {
            UiDrawCommand& last = commands.back();
            if (last.type == type
                && last.textureID == textureID
                && last.indexOffset + last.indexCount == indexOffset)
            {
                last.indexCount += indexCount;
                return;
            }
        }

        commands.push_back({type, textureID, indexOffset, indexCount});
    }

    void addTexturedQuadUv(float x, float y, float w, float h,
                           texture_id_type texID,
                           glm::vec2 uvMin,
                           glm::vec2 uvMax,
                           glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({{x,     y    }, {uvMin.x, uvMin.y}, tint});
        vertices.push_back({{x + w, y    }, {uvMax.x, uvMin.y}, tint});
        vertices.push_back({{x,     y + h}, {uvMin.x, uvMax.y}, tint});
        vertices.push_back({{x + w, y + h}, {uvMax.x, uvMax.y}, tint});

        const uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base + 0); indices.push_back(base + 2);
        indices.push_back(base + 1); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 3);

        addDrawCommand(UiDrawType::TexturedQuad, texID, idxBase, 6);
    }

    void addTexturedQuadUvRotated(float x, float y, float w, float h,
                                  texture_id_type texID,
                                  glm::vec2 uvMin,
                                  glm::vec2 uvMax,
                                  float rotation,
                                  glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        const float centerX = x + w * 0.5f;
        const float centerY = y + h * 0.5f;
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);

        auto rotatePoint = [&](float px, float py) -> glm::vec2
        {
            const float localX = px - centerX;
            const float localY = py - centerY;
            return {
                centerX + localX * c - localY * s,
                centerY + localX * s + localY * c
            };
        };

        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({rotatePoint(x,     y),     {uvMin.x, uvMin.y}, tint});
        vertices.push_back({rotatePoint(x + w, y),     {uvMax.x, uvMin.y}, tint});
        vertices.push_back({rotatePoint(x,     y + h), {uvMin.x, uvMax.y}, tint});
        vertices.push_back({rotatePoint(x + w, y + h), {uvMax.x, uvMax.y}, tint});

        const uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base + 0); indices.push_back(base + 2);
        indices.push_back(base + 1); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 3);

        addDrawCommand(UiDrawType::TexturedQuad, texID, idxBase, 6);
    }

    void addTexturedQuad(float x, float y, float w, float h,
                         texture_id_type texID,
                         glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        addTexturedQuadUv(x, y, w, h, texID, {0.0f, 0.0f}, {1.0f, 1.0f}, tint);
    }

    void addGlyphBatch(const std::vector<UiVertex>& glyphVerts,
                       const std::vector<uint32_t>& glyphIndices,
                       texture_id_type atlasTexture)
    {
        if (glyphVerts.empty()) return;

        const uint32_t vertBase = static_cast<uint32_t>(vertices.size());
        const uint32_t idxBase  = static_cast<uint32_t>(indices.size());

        vertices.insert(vertices.end(), glyphVerts.begin(), glyphVerts.end());
        for (uint32_t idx : glyphIndices)
            indices.push_back(vertBase + idx);

        addDrawCommand(UiDrawType::TexturedQuad,
                       atlasTexture,
                       idxBase,
                       static_cast<uint32_t>(glyphIndices.size()));
    }

    void addColoredQuad(float x, float y, float w, float h, glm::vec4 color)
    {
        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({{x,     y    }, {0.0f, 0.0f}, color});
        vertices.push_back({{x + w, y    }, {1.0f, 0.0f}, color});
        vertices.push_back({{x,     y + h}, {0.0f, 1.0f}, color});
        vertices.push_back({{x + w, y + h}, {1.0f, 1.0f}, color});

        const uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base + 0); indices.push_back(base + 2);
        indices.push_back(base + 1); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 3);

        addDrawCommand(UiDrawType::ColoredQuad, 0, idxBase, 6);
    }

    void addColoredPolygon(const std::vector<glm::vec2>& points, glm::vec4 color)
    {
        if (points.size() < 3)
            return;

        glm::vec2 center{0.0f, 0.0f};
        for (glm::vec2 point : points)
            center += point;
        center /= static_cast<float>(points.size());

        const uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({center, {0.5f, 0.5f}, color});
        for (glm::vec2 point : points)
            vertices.push_back({point, {0.0f, 0.0f}, color});

        const uint32_t idxBase = static_cast<uint32_t>(indices.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(points.size()); ++i)
        {
            const uint32_t next = (i + 1) % static_cast<uint32_t>(points.size());
            indices.push_back(base);
            indices.push_back(base + 1 + i);
            indices.push_back(base + 1 + next);
        }

        addDrawCommand(UiDrawType::ColoredQuad, 0, idxBase, static_cast<uint32_t>(points.size() * 3));
    }

    void addColoredLine(glm::vec2 start, glm::vec2 end, float thickness, glm::vec4 color)
    {
        const glm::vec2 delta = end - start;
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length <= 0.0f || thickness <= 0.0f)
            return;

        const glm::vec2 normal = {-delta.y / length, delta.x / length};
        const glm::vec2 offset = normal * (thickness * 0.5f);
        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({start - offset, {0.0f, 0.0f}, color});
        vertices.push_back({start + offset, {1.0f, 0.0f}, color});
        vertices.push_back({end - offset,   {0.0f, 1.0f}, color});
        vertices.push_back({end + offset,   {1.0f, 1.0f}, color});

        const uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base + 0); indices.push_back(base + 2);
        indices.push_back(base + 1); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 3);

        addDrawCommand(UiDrawType::ColoredQuad, 0, idxBase, 6);
    }
};
