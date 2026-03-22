#pragma once

#include <vector>

#include "GlmConfig.h"

#include "Types.h"

// Per-vertex data for a text glyph quad.
// pos   : normalized screen coordinates [0..1] or pre-transformed NDC
// uv    : atlas UV coordinate [0..1]
// color : RGBA tint, default white (1,1,1,1)
//         Multiplied with the atlas sample in the fragment shader.
//         Use for colored text or future solid-color quads.
struct TextGlyphVertex
{
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

// One draw batch for a single font atlas texture.
// TextRenderStage uploads all batches into one vertex buffer and issues
// one draw call per batch (one per unique atlas texture).
struct TextCommandList
{
    texture_id_type              textureID = 0;
    std::vector<TextGlyphVertex> vertices;
    std::vector<uint32_t>        indices;
};
