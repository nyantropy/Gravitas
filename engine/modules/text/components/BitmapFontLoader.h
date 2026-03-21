#pragma once

#include <string>

#include "BitmapFont.h"
#include "IResourceProvider.hpp"

// Utility for loading a uniform-grid bitmap font atlas.
// Each row of cells shares the same width/height; characters are packed
// left-to-right, top-to-bottom in the atlas following charOrder.
//
// All glyphs receive uniform monospaced metrics:
//   size = (1, 1) world units, bearing = (0, 1), advance = 1.0.
namespace BitmapFontLoader
{
    // atlasW/atlasH  : atlas dimensions in pixels
    // cellW/cellH    : glyph cell size in pixels
    // cols           : cells per row in the atlas
    // charOrder      : characters listed in atlas order (null terminator not counted)
    // lineHeight     : vertical advance per line in world units
    // pixelSampling  : true → nearest-neighbour (requestPixelTexture); false → bilinear
    inline BitmapFont load(
        IResourceProvider* resources,
        const std::string& atlasPath,
        int atlasW, int atlasH,
        int cellW, int cellH, int cols,
        const std::string& charOrder,
        float lineHeight,
        bool pixelSampling = true)
    {
        BitmapFont font;
        font.atlasTexture = pixelSampling
            ? resources->requestPixelTexture(atlasPath)
            : resources->requestTexture(atlasPath);
        font.lineHeight = lineHeight;

        const float fW = static_cast<float>(atlasW);
        const float fH = static_cast<float>(atlasH);
        const int   charCount = static_cast<int>(charOrder.size());

        for (int i = 0; i < charCount; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;

            const int ax0 = col * cellW;
            const int ay0 = row * cellH;
            const int ax1 = ax0 + cellW - 1;
            const int ay1 = ay0 + cellH - 1;

            font.glyphs[charOrder[i]] = GlyphInfo{
                .uvMin   = { ax0 / fW,        ay0 / fH        },
                .uvMax   = { (ax1 + 1) / fW, (ay1 + 1) / fH  },
                .size    = { 1.0f, 1.0f },
                .bearing = { 0.0f, 1.0f },
                .advance = 1.0f
            };
        }

        return font;
    }
}
