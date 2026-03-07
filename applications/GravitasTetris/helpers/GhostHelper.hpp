#pragma once

// computeDropPivot (the cast-down algorithm) lives in TetrisPhysicsHelper so it
// can be shared with hard drop without duplication.
#include "TetrisPhysicsHelper.hpp"
#include "TetrominoShape.hpp"
#include "TetrisBlockComponent.hpp"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <array>

// Repositions the four persistent ghost block ECS entities to the computed landing position.
// Also syncs b.type each frame so TetrisVisualSystem can rebind the texture when the piece changes.
inline void updateGhostBlocks(
    ECSWorld&                     world,
    const TetrisGrid&             grid,
    const ActiveTetromino&        active,
    const std::array<Entity, 4>&  ghostBlocks)
{
    glm::ivec2 ghostPivot = computeDropPivot(grid, active);
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
