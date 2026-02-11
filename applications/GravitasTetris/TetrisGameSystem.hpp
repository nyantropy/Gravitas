#pragma once

#ifdef __INTELLISENSE__
#pragma diag_suppress 70
#pragma diag_suppress 2370
#endif

#include <random>
#include <glm.hpp>
#include <vector>

#include "ECSSimulationSystem.hpp"
#include "TetrisGrid.hpp"
#include "TetrominoType.hpp"
#include "TetrominoShape.hpp"
#include "TetrisInputState.hpp"
#include "ActiveTetromino.hpp"
#include "Entity.h"
#include "TetrisBlockComponent.hpp"

// the logical core of the tetris game
// it operates on the ecs, but only ever works on the TetrisBlockComponent class to adjust values, which will correctly reflect the
// gamestate on screen - it also uses its own grid for keeping track of what spaces are occupied, and what spaces are not, and keeps the id
// of each block, so they can be destroyed when a line does clear out
class TetrisGameSystem : public ECSSimulationSystem
{
    private:
        bool firstUpdate = true;

        TetrisGrid grid;
        ActiveTetromino active;
        TetrisInputState& input;

        float fallTimer = 0.0f;
        float fallInterval = 0.7f;

    public:
        TetrisGameSystem(TetrisInputState& in)
            : input(in)
        {
        }

        // update tick, this is where all the magic happens
        void update(ECSWorld& world, float dt) override
        {
            rebuildGrid(world);

            // hacky solution, always checking for first update is dumb, but it works so ill leave it for now
            if (firstUpdate)
            {
                spawnPiece(world);
                firstUpdate = false;
            }

            handleInput(world);

            fallTimer += dt;
            float interval = input.softDrop ? 0.05f : fallInterval;
            if (fallTimer >= interval)
            {
                tryMove(world, { 0, -1 });
                fallTimer = 0.0f;
            }
        }

    private:
        // the grid serves as a cache and as a way to track when a line clear occurs
        // as such it needs to be rebuilt often
        void rebuildGrid(ECSWorld& world)
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

        // input handling is simple, we have a separate system that catches whatever keys are pressed, and this simply executes the right move
        void handleInput(ECSWorld& world)
        {
            if (input.moveLeft)  tryMove(world, { -1, 0 });
            if (input.moveRight) tryMove(world, {  1, 0 });
            if (input.rotate)    tryRotate(world);
        }

        bool testPosition(ECSWorld& world, glm::ivec2 newPivot, int newRot) const
        {
            auto& shape = TetrominoShapes[(int)active.type][newRot];
            for (int i = 0; i < 4; ++i)
            {
                int x = newPivot.x + shape.blocks[i].x;
                int y = newPivot.y + shape.blocks[i].y;
                if (!grid.inBounds(x, y) || grid.occupied(x, y))
                    return false;
            }
            return true;
        }

        void applyToActiveBlocks(ECSWorld& world)
        {
            auto& shape = TetrominoShapes[(int)active.type][active.rotation];
            for (int i = 0; i < 4; ++i)
            {
                auto& b = world.getComponent<TetrisBlockComponent>(active.blocks[i]);
                glm::ivec2 p = active.pivot + shape.blocks[i];
                b.x = p.x;
                b.y = p.y;
            }
        }

        // try to move the active piece
        void tryMove(ECSWorld& world, glm::ivec2 delta)
        {
            glm::ivec2 newPivot = active.pivot + delta;
            if (testPosition(world, newPivot, active.rotation))
            {
                active.pivot = newPivot;
                applyToActiveBlocks(world);
                return;
            }

            if (delta.y == -1)
            {
                lockPiece(world);
                rebuildGrid(world);
                clearLines(world);
                rebuildGrid(world);
                spawnPiece(world);
            }
        }

        // try to rotate the active piece
        void tryRotate(ECSWorld& world)
        {
            int newRot = (active.rotation + 1) % 4;
            if (!testPosition(world, active.pivot, newRot))
                return;

            active.rotation = newRot;
            applyToActiveBlocks(world);
        }

        // lock the active piece on the grid
        void lockPiece(ECSWorld& world)
        {
            for (int i = 0; i < 4; ++i)
            {
                auto& b = world.getComponent<TetrisBlockComponent>(active.blocks[i]);
                b.active = false;
            }
        }

        // try to clear lines, only actually deletes anything when a row is full of blocks
        void clearLines(ECSWorld& world)
        {
            for (int y = 0; y < grid.height; ++y)
            {
                bool full = true;
                for (int x = 0; x < grid.width; ++x)
                    if (!grid.occupied(x, y))
                        full = false;

                if (!full) continue;

                std::vector<Entity> toDestroy;
                for (int x = 0; x < grid.width; ++x)
                {
                    Entity e = grid.at(x, y);
                    if (e != Entity{ UINT32_MAX })
                        toDestroy.push_back(e);
                }

                int destroy = 1;
                for (Entity e : toDestroy)
                {
                    world.destroyEntity(e);
                    destroy++;
                }

                world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& b)
                {
                    if (!b.active && b.y > y)
                        b.y -= 1;
                });

                y--;
                rebuildGrid(world);
            }
            
            
        }

        // spawn a new, random tetris piece at the top of the grid
        void spawnPiece(ECSWorld& world)
        {
            active.type = (TetrominoType)(rand() % 7);
            active.rotation = 0;
            active.pivot = { grid.width / 2, grid.height - 1 };

            auto& shape = TetrominoShapes[(int)active.type][0];
            for (int i = 0; i < 4; ++i)
            {
                Entity e = world.createEntity();
                world.addComponent(e, TetrisBlockComponent{ 0, 0, true });
                active.blocks[i] = e;
            }

            applyToActiveBlocks(world);
        }
};
