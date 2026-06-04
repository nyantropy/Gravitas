#pragma once

#include <cstdint>
#include <string>

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
};
