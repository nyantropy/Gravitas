#pragma once

#include <string>

#include "BitmapFont.h"
#include "FontAsset.h"
#include "IResourceProvider.hpp"

// Utility for loading a uniform-grid bitmap font atlas.
// Each row of cells shares the same width/height; characters are packed
// left-to-right, top-to-bottom in the atlas following charOrder.
//
// All glyphs receive uniform monospaced metrics normalized to cell height:
//   size = (cellW / cellH, 1), bearing = (0, 1), advance = cellW / cellH.
namespace BitmapFontLoader
{
    inline BitmapFont buildGridFont(texture_id_type atlasTexture,
                                    int atlasW,
                                    int atlasH,
                                    int cellW,
                                    int cellH,
                                    int cols,
                                    const std::string& charOrder,
                                    float lineHeight)
    {
        BitmapFont font;
        font.atlasTexture = atlasTexture;
        font.lineHeight = lineHeight;

        const float fW = static_cast<float>(atlasW);
        const float fH = static_cast<float>(atlasH);
        const float cellAspect = static_cast<float>(cellW) / static_cast<float>(cellH);
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
                .size    = { cellAspect, 1.0f },
                .bearing = { 0.0f, 1.0f },
                .advance = cellAspect
            };
        }

        return font;
    }

    inline BitmapFont buildGridFont(texture_id_type atlasTexture, const FontAsset& asset)
    {
        return buildGridFont(atlasTexture,
                             static_cast<int>(asset.atlasWidth),
                             static_cast<int>(asset.atlasHeight),
                             static_cast<int>(asset.cellWidth),
                             static_cast<int>(asset.cellHeight),
                             static_cast<int>(asset.columns),
                             asset.charOrder,
                             asset.lineHeight);
    }

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
        const texture_id_type atlasTexture = pixelSampling
            ? resources->requestPixelTexture(atlasPath)
            : resources->requestTexture(atlasPath);
        return buildGridFont(atlasTexture, atlasW, atlasH, cellW, cellH, cols, charOrder, lineHeight);
    }
}
