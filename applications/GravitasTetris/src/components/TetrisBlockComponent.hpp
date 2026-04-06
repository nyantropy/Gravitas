#pragma once

#include "struct/TetrominoType.hpp"

// the component used to make most tetris interactions possible
struct TetrisBlockComponent
{
    int x;
    int y;
    bool active;

    TetrominoType type;
};

