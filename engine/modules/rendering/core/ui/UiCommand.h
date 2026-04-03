#pragma once

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

    void addTexturedQuad(float x, float y, float w, float h,
                         texture_id_type texID,
                         glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({{x,     y    }, {0.0f, 0.0f}, tint});
        vertices.push_back({{x + w, y    }, {1.0f, 0.0f}, tint});
        vertices.push_back({{x,     y + h}, {0.0f, 1.0f}, tint});
        vertices.push_back({{x + w, y + h}, {1.0f, 1.0f}, tint});

        const uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base + 0); indices.push_back(base + 2);
        indices.push_back(base + 1); indices.push_back(base + 1);
        indices.push_back(base + 2); indices.push_back(base + 3);

        commands.push_back({UiDrawType::TexturedQuad, texID, idxBase, 6});
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

        commands.push_back({
            UiDrawType::TexturedQuad,
            atlasTexture,
            idxBase,
            static_cast<uint32_t>(glyphIndices.size())
        });
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

        commands.push_back({UiDrawType::ColoredQuad, 0, idxBase, 6});
    }
};
