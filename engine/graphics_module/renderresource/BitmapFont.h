#pragma once

#include <unordered_map>

#include <glm.hpp>

#include "Types.h"

// UV rectangle + layout metrics for a single glyph in a bitmap atlas.
// All size/bearing/advance values are in pixels (atlas space); scale is
// applied later by TextBuildSystem when generating world-space quads.
struct GlyphInfo
{
    glm::vec2 uvMin;     // top-left UV in the atlas  [0..1]
    glm::vec2 uvMax;     // bottom-right UV in the atlas [0..1]
    glm::vec2 size;      // glyph width and height in pixels
    glm::vec2 bearing;   // offset from baseline to top-left of glyph (x=right, y=up)
    float     advance;   // horizontal advance to next glyph origin
};

// A loaded bitmap font: a GPU texture atlas plus per-character metrics.
// The atlasTexture ID is obtained by calling IResourceProvider::requestTexture()
// before constructing the BitmapFont, and stored here so TextBindingSystem
// can set it directly on RenderGpuComponent without going through a path string.
struct BitmapFont
{
    texture_id_type                   atlasTexture;
    std::unordered_map<char, GlyphInfo> glyphs;
    float                             lineHeight;
};
