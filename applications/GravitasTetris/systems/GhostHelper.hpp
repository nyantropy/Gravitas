#pragma once

#include "TetrisPhysicsHelper.hpp"
#include "ActiveTetromino.hpp"
#include "TetrominoShape.hpp"
#include "TetrisBlockComponent.hpp"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <array>
#include <glm.hpp>

// Casts the active piece straight down to find the lowest valid pivot row.
inline glm::ivec2 computeGhostPivot(const TetrisGrid& grid, const ActiveTetromino& active)
{
    glm::ivec2 ghostPivot = active.pivot;
    while (testPosition(grid, active.type, { ghostPivot.x, ghostPivot.y - 1 }, active.rotation))
        ghostPivot.y -= 1;
    return ghostPivot;
}

// Repositions the four persistent ghost block ECS entities to the landing position.
// Also syncs b.type so TetrisVisualSystem can update the texture when the piece changes.
inline void updateGhostBlocks(
    ECSWorld&                     world,
    const TetrisGrid&             grid,
    const ActiveTetromino&        active,
    const std::array<Entity, 4>&  ghostBlocks)
{
    glm::ivec2 ghostPivot = computeGhostPivot(grid, active);
    auto& shape = TetrominoShapes[(int)active.type][active.rotation];

    for (int i = 0; i < 4; ++i)
    {
        auto& b      = world.getComponent<TetrisBlockComponent>(ghostBlocks[i]);
        glm::ivec2 p = ghostPivot + shape.blocks[i];
        b.x    = p.x;
        b.y    = p.y;
        b.type = active.type;
    }
}
