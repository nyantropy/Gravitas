#pragma once

#include <string>

#include "struct/TetrominoType.hpp"

static const std::string GtsTetrisGamePath = ".";

inline std::string gtsTetrisResourcePath(const std::string& filename)
{
    return GtsTetrisGamePath + "/resources/" + filename;
}

inline std::string gtsTetrisTexturePath(TetrominoType type)
{
    switch (type)
    {
        case TetrominoType::I: return gtsTetrisResourcePath("light_blue_texture.png");
        case TetrominoType::O: return gtsTetrisResourcePath("yellow_texture.png");
        case TetrominoType::T: return gtsTetrisResourcePath("purple_texture.png");
        case TetrominoType::S: return gtsTetrisResourcePath("green_texture.png");
        case TetrominoType::Z: return gtsTetrisResourcePath("red_texture.png");
        case TetrominoType::J: return gtsTetrisResourcePath("dark_blue_texture.png");
        case TetrominoType::L: return gtsTetrisResourcePath("orange_texture.png");
        default:               return gtsTetrisResourcePath("yellow_texture.png");
    }
}
