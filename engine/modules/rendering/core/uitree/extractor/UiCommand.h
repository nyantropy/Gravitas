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

// The type of a UI draw command.
enum class UiDrawType
{
    ColoredQuad,   // solid color rectangle, no texture
    TexturedQuad   // image/sprite with optional tint color
};

// One draw command referencing a range in UiCommandBuffer's vertex/index arrays.
struct UiDrawCommand
{
    UiDrawType      type        = UiDrawType::ColoredQuad;
    texture_id_type textureID   = 0;
    uint32_t        indexOffset = 0;
    uint32_t        indexCount  = 0;
};

// All UI draw data for one frame.
// UiRenderStage uploads vertices/indices once and issues one draw call per
// UiDrawCommand, binding the appropriate texture and pushing useTexture.
struct UiCommandBuffer
{
    std::vector<UiVertex>      vertices;
    std::vector<uint32_t>      indices;
    std::vector<UiDrawCommand> commands;

    bool empty() const { return commands.empty(); }

    // Helper: append a textured quad at normalized screen position.
    // (x, y) = top-left, (w, h) = size, all in [0..1] viewport coords.
    void addTexturedQuad(float x, float y, float w, float h,
                         texture_id_type texID,
                         glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({{x,     y    }, {0.0f, 0.0f}, tint}); // TL
        vertices.push_back({{x+w,   y    }, {1.0f, 0.0f}, tint}); // TR
        vertices.push_back({{x,     y+h  }, {0.0f, 1.0f}, tint}); // BL
        vertices.push_back({{x+w,   y+h  }, {1.0f, 1.0f}, tint}); // BR

        uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base+0); indices.push_back(base+2);
        indices.push_back(base+1); indices.push_back(base+1);
        indices.push_back(base+2); indices.push_back(base+3);

        commands.push_back({UiDrawType::TexturedQuad, texID, idxBase, 6});
    }

    // Helper: append a batch of glyph quads sharing one atlas texture.
    void addGlyphBatch(const std::vector<UiVertex>& glyphVerts,
                       const std::vector<uint32_t>& glyphIndices,
                       texture_id_type atlasTexture)
    {
        if (glyphVerts.empty()) return;
        uint32_t vertBase = static_cast<uint32_t>(vertices.size());
        uint32_t idxBase  = static_cast<uint32_t>(indices.size());
        vertices.insert(vertices.end(), glyphVerts.begin(), glyphVerts.end());
        for (uint32_t idx : glyphIndices)
            indices.push_back(vertBase + idx);
        commands.push_back({UiDrawType::TexturedQuad, atlasTexture, idxBase,
                            static_cast<uint32_t>(glyphIndices.size())});
    }

    // Helper: append a solid color quad.
    void addColoredQuad(float x, float y, float w, float h, glm::vec4 color)
    {
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({{x,   y  }, {0.0f, 0.0f}, color});
        vertices.push_back({{x+w, y  }, {1.0f, 0.0f}, color});
        vertices.push_back({{x,   y+h}, {0.0f, 1.0f}, color});
        vertices.push_back({{x+w, y+h}, {1.0f, 1.0f}, color});

        uint32_t idxBase = static_cast<uint32_t>(indices.size());
        indices.push_back(base+0); indices.push_back(base+2);
        indices.push_back(base+1); indices.push_back(base+1);
        indices.push_back(base+2); indices.push_back(base+3);

        commands.push_back({UiDrawType::ColoredQuad, 0, idxBase, 6});
    }
};
