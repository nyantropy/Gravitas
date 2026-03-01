#pragma once

#include "TetrisGrid.hpp"
#include "TetrominoShape.hpp"
#include "TetrominoType.hpp"
#include "TetrisBlockComponent.hpp"
#include "ECSWorld.hpp"
#include <glm.hpp>

// Returns true if the given piece fits at pivot/rotation without colliding or leaving the grid.
inline bool testPosition(const TetrisGrid& grid, TetrominoType type, glm::ivec2 pivot, int rotation)
{
    auto& shape = TetrominoShapes[(int)type][rotation];

    for (int i = 0; i < 4; ++i)
    {
        int x = pivot.x + shape.blocks[i].x;
        int y = pivot.y + shape.blocks[i].y;

        if (x < 0 || x >= grid.width || y < 0)
            return false;

        if (y < grid.height && grid.occupied(x, y))
            return false;
    }

    return true;
}

// Rebuilds the spatial grid cache from the current ECS world state.
// Only locked (active == false) blocks are written into the grid.
inline void rebuildGrid(ECSWorld& world, TetrisGrid& grid)
{
    for (auto& c : grid.cells)
        c = Entity{ UINT32_MAX };

    world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& b)
    {
        if (!grid.inBounds(b.x, b.y))
            return;
        if (!b.active)
            grid.at(b.x, b.y) = e;
    });
}
