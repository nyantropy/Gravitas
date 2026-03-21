#pragma once

#include "TetrisGrid.hpp"
#include "TetrominoShape.hpp"
#include "TetrominoType.hpp"
#include "ActiveTetromino.hpp"
#include "GlmConfig.h"

// Pure physics algorithms shared across multiple controllers.
namespace TetrisPhysics
{
    // Returns true if the given piece fits at pivot/rotation without colliding
    // with locked blocks or leaving the grid bounds.
    inline bool testPosition(
        const TetrisGrid& grid,
        TetrominoType     type,
        glm::ivec2        pivot,
        int               rotation)
    {
        auto& shape = TetrominoShapes[(int)type][rotation];
        for (int i = 0; i < 4; ++i)
        {
            int x = pivot.x + shape.blocks[i].x;
            int y = pivot.y + shape.blocks[i].y;
            if (x < 0 || x >= grid.width || y < 0) return false;
            if (y < grid.height && grid.occupied(x, y)) return false;
        }
        return true;
    }

    // Casts a piece straight down from its current pivot to find the lowest
    // valid row.  Used for both hard drop and ghost projection.
    inline glm::ivec2 computeDropPivot(const TetrisGrid& grid, const ActiveTetromino& active)
    {
        glm::ivec2 drop = active.pivot;
        while (testPosition(grid, active.type, { drop.x, drop.y - 1 }, active.rotation))
            drop.y -= 1;
        return drop;
    }
}
