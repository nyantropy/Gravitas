#pragma once

#include "TetrominoType.hpp"
#include "Entity.h"
#include <glm.hpp>
#include <array>

// represents the active tetromino on the grid as a cache object, the actual data is inside the ecs
struct ActiveTetromino
{
    TetrominoType type;
    int rotation = 0;
    glm::ivec2 pivot;
    std::array<Entity,4> blocks;
};
