#pragma once

#include "ECSWorld.hpp"
#include "TetrominoType.hpp"
#include "TetrominoShape.hpp"
#include "TetrisBlockComponent.hpp"
#include "NextPieceBlockComponent.hpp"
#include "Entity.h"
#include <array>
#include <glm.hpp>

// Creates the four ECS entities for a single next-piece queue slot.
// Blocks are positioned immediately using the rotation-0 shape at 'pivot'.
// Returns the entity array; ownership passes to the caller (TetrisGameSystem).
inline std::array<Entity, 4> spawnPreviewPiece(ECSWorld& world, TetrominoType type, glm::ivec2 pivot)
{
    auto& shape = TetrominoShapes[(int)type][0];
    std::array<Entity, 4> slot;

    for (int j = 0; j < 4; ++j)
    {
        glm::ivec2 p = pivot + shape.blocks[j];
        Entity e = world.createEntity();
        world.addComponent(e, TetrisBlockComponent{ p.x, p.y, true, type });
        world.addComponent(e, NextPieceBlockComponent{});
        slot[j] = e;
    }

    return slot;
}
