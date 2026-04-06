#pragma once

#include <string_view>

namespace RetroFontAtlas
{
    inline constexpr int ATLAS_W = 64;
    inline constexpr int ATLAS_H = 96;
    inline constexpr int CELL_W = 8;
    inline constexpr int CELL_H = 8;
    inline constexpr int ATLAS_COLS = 8;
    inline constexpr float LINE_HEIGHT = 1.2f;
    inline constexpr std::string_view CHAR_ORDER =
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
}
