#pragma once

#include "TetrisGrid.hpp"
#include "ActiveTetromino.hpp"
#include "TetrisBlockComponent.hpp"
#include "GhostBlockComponent.hpp"
#include "TetrisPhysics.hpp"
#include "TetrominoShape.hpp"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <array>
#include <glm.hpp>

// Owns the four ghost-block ECS entities and keeps them projected to the
// piece's landing row every frame.
struct GhostController
{
    void init(ECSWorld& world, const TetrisGrid& grid, const ActiveTetromino& active)
    {
        for (int i = 0; i < 4; ++i)
        {
            Entity e = world.createEntity();
            world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
            world.addComponent(e, GhostBlockComponent{});
            blocks[i] = e;
        }
        sync(world, grid, active);
    }

    // Repositions ghost blocks to the landing row and syncs piece type for texture rebinding.
    void sync(ECSWorld& world, const TetrisGrid& grid, const ActiveTetromino& active)
    {
        glm::ivec2 ghostPivot = TetrisPhysics::computeDropPivot(grid, active);
        auto& shape = TetrominoShapes[(int)active.type][active.rotation];
        for (int i = 0; i < 4; ++i)
        {
            auto& b      = world.getComponent<TetrisBlockComponent>(blocks[i]);
            glm::ivec2 p = ghostPivot + shape.blocks[i];
            b.x    = p.x;
            b.y    = p.y;
            b.type = active.type;
        }
    }

private:
    std::array<Entity, 4> blocks;
};
