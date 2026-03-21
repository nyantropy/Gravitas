#pragma once

#include <vector>

#include <glm.hpp>

#include "Types.h"

// Per-vertex data for a UI glyph quad.
// Positions are in normalised viewport space: (0,0) = top-left, (1,1) = bottom-right.
struct UIGlyphVertex
{
    glm::vec2 pos;  // normalised screen position
    glm::vec2 uv;   // atlas UV coordinate
};

// One draw batch for a single font atlas.
// UICommandExtractor produces one list per unique textureID referenced by
// visible UITextComponents.  The Vulkan UI renderer issues one draw call per list.
struct UICommandList
{
    texture_id_type            textureID = 0;
    std::vector<UIGlyphVertex> vertices;
    std::vector<uint32_t>      indices;
};
