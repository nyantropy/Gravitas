#pragma once

#include <array>
#include <glm.hpp>

// defines a tetromino shape in blocks
struct TetrominoShape
{
    std::array<glm::ivec2, 4> blocks;
};

extern const TetrominoShape TetrominoShapes[7][4];