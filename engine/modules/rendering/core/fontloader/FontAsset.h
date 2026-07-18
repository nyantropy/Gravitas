#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct FontGlyphMetrics
{
    float sizeX = 0.0f;
    float sizeY = 0.0f;
    float advance = 0.0f;
};

struct FontAsset
{
    std::string atlasPath;
    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;
    uint32_t cellWidth = 0;
    uint32_t cellHeight = 0;
    uint32_t columns = 1;
    std::string charOrder;
    float lineHeight = 1.0f;
    bool pixelSampling = true;
    FontGlyphMetrics glyphDefaults;
    std::unordered_map<char, FontGlyphMetrics> glyphOverrides;
};
