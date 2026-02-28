#pragma once

#include "TetrominoType.hpp"

// the component used to make most tetris interactions possible
struct TetrisBlockComponent
{
    int x;
    int y;
    bool active;

    TetrominoType type;
};

