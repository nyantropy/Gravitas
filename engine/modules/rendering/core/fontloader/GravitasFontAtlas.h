#pragma once

#include <string_view>

namespace GravitasFontAtlas
{
    inline constexpr int ATLAS_W = 128;
    inline constexpr int ATLAS_H = 192;
    inline constexpr int CELL_W = 16;
    inline constexpr int CELL_H = 16;
    inline constexpr int ATLAS_COLS = 8;
    inline constexpr float LINE_HEIGHT = 1.2f;
    inline constexpr std::string_view CHAR_ORDER =
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
}
