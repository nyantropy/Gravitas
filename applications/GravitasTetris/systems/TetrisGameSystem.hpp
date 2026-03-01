#pragma once

#ifdef __INTELLISENSE__
#pragma diag_suppress 70
#pragma diag_suppress 2370
#endif

#include <random>
#include <glm.hpp>
#include <vector>
#include <deque>
#include <array>

#include "ECSSimulationSystem.hpp"
#include "TetrisGrid.hpp"
#include "TetrominoType.hpp"
#include "TetrominoShape.hpp"
#include "TetrisInputComponent.hpp"
#include "ActiveTetromino.hpp"
#include "Entity.h"
#include "TetrisBlockComponent.hpp"
#include "NextPieceBlockComponent.hpp"
#include "GhostBlockComponent.hpp"
#include "RenderResourceClearComponent.h"
#include "TetrisScoreComponent.hpp"

#include "TetrisPhysicsHelper.hpp"
#include "WallKickHelper.hpp"
#include "GhostHelper.hpp"
#include "LineClearHelper.hpp"

// The logical core of the Tetris game.
// Orchestrates ECS state each tick by delegating feature logic to helper functions.
// Grid collision (testPosition, rebuildGrid), rotation (tryWallKick), ghost projection
// (updateGhostBlocks), and line clearing (clearLines) all live in their own headers.
class TetrisGameSystem : public ECSSimulationSystem
{
    private:
        bool firstUpdate = true;

        TetrisGrid     grid;
        ActiveTetromino active;

        float fallTimer    = 0.0f;
        float fallInterval = 0.4f;

        float moveTimer    = 0.0f;
        float moveInterval = 0.08f;

        float rotateTimer    = 0.0f;
        float rotateInterval = 0.15f;

        std::deque<TetrominoType>           nextQueue;
        std::vector<std::array<Entity, 4>>  previewBlocks;
        std::array<Entity, 4>               ghostBlocks;

    public:
        // Number of upcoming pieces shown in the queue.  Set to 0 to disable.
        static constexpr int QUEUE_SIZE = 4;

        TetrisGameSystem() {}

        void update(ECSWorld& world, float dt) override
        {
            rebuildGrid(world, grid);

            if (firstUpdate)
            {
                initQueue(world);
                spawnPiece(world);
                initGhost(world);   // must come after spawnPiece so active.type is set
                firstUpdate = false;
            }

            moveTimer   += dt;
            rotateTimer += dt;

            handleInput(world);

            auto& input = world.getSingleton<TetrisInputComponent>();

            fallTimer += dt;
            float interval = input.softDrop ? 0.05f : fallInterval;
            if (fallTimer >= interval)
            {
                tryMove(world, { 0, -1 });
                fallTimer = 0.0f;
            }

            input.clear();
            updateGhostBlocks(world, grid, active, ghostBlocks);
        }


    private:
        void handleInput(ECSWorld& world)
        {
            auto& input = world.getSingleton<TetrisInputComponent>();

            // Hard drop: single-shot, resets fall timer internally
            if (input.hardDrop)
            {
                hardDrop(world);
                return;   // skip movement/rotation processing this frame
            }

            if (input.moveLeft && moveTimer >= moveInterval)
            {
                tryMove(world, { -1, 0 });
                moveTimer = 0.0f;
            }

            if (input.moveRight && moveTimer >= moveInterval)
            {
                tryMove(world, { 1, 0 });
                moveTimer = 0.0f;
            }

            // CW and CCW share the same debounce timer; CW takes precedence if both held
            if (rotateTimer >= rotateInterval)
            {
                if (input.rotateCW)
                {
                    if (auto result = tryWallKick(grid, active, +1))
                    {
                        active.pivot    = result->pivot;
                        active.rotation = result->rotation;
                        applyToActiveBlocks(world);
                    }
                    rotateTimer = 0.0f;
                }
                else if (input.rotateCCW)
                {
                    if (auto result = tryWallKick(grid, active, -1))
                    {
                        active.pivot    = result->pivot;
                        active.rotation = result->rotation;
                        applyToActiveBlocks(world);
                    }
                    rotateTimer = 0.0f;
                }
            }
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

        void tryMove(ECSWorld& world, glm::ivec2 delta)
        {
            glm::ivec2 newPivot = active.pivot + delta;
            if (testPosition(grid, active.type, newPivot, active.rotation))
            {
                active.pivot = newPivot;
                applyToActiveBlocks(world);
                return;
            }

            if (delta.y == -1)
            {
                lockPiece(world);
                rebuildGrid(world, grid);
                clearLines(world, grid);
                rebuildGrid(world, grid);
                spawnPiece(world);
            }
        }

        void lockPiece(ECSWorld& world)
        {
            for (int i = 0; i < 4; ++i)
            {
                auto& b = world.getComponent<TetrisBlockComponent>(active.blocks[i]);
                b.active = false;
            }
        }

        void hardDrop(ECSWorld& world)
        {
            while (testPosition(grid, active.type, { active.pivot.x, active.pivot.y - 1 }, active.rotation))
                active.pivot.y -= 1;

            applyToActiveBlocks(world);

            lockPiece(world);
            rebuildGrid(world, grid);
            clearLines(world, grid);
            rebuildGrid(world, grid);
            spawnPiece(world);

            fallTimer = 0.0f;   // prevent immediate gravity tick on the new piece
        }

        // ---------------------------------------------------------------
        // Queue management
        // ---------------------------------------------------------------

        void initQueue(ECSWorld& world)
        {
            if (QUEUE_SIZE <= 0) return;

            nextQueue.clear();
            previewBlocks.resize(QUEUE_SIZE);

            for (int i = 0; i < QUEUE_SIZE; ++i)
            {
                TetrominoType t = (TetrominoType)(rand() % 7);
                nextQueue.push_back(t);

                for (int j = 0; j < 4; ++j)
                {
                    Entity e = world.createEntity();
                    world.addComponent(e, TetrisBlockComponent{ 0, 0, true, t });
                    world.addComponent(e, NextPieceBlockComponent{});
                    previewBlocks[i][j] = e;
                }
            }

            updatePreviews(world);
        }

        void updatePreviews(ECSWorld& world)
        {
            if (QUEUE_SIZE <= 0) return;

            for (int i = 0; i < (int)previewBlocks.size(); ++i)
            {
                TetrominoType t  = nextQueue[i];
                glm::ivec2 pivot = { 13, 12 - i * 3 };
                auto& shape      = TetrominoShapes[(int)t][0];

                for (int j = 0; j < 4; ++j)
                {
                    auto& b = world.getComponent<TetrisBlockComponent>(previewBlocks[i][j]);
                    glm::ivec2 p = pivot + shape.blocks[j];
                    b.x = p.x;
                    b.y = p.y;
                }
            }
        }

        // ---------------------------------------------------------------
        // Ghost block management
        // ---------------------------------------------------------------

        void initGhost(ECSWorld& world)
        {
            for (int i = 0; i < 4; ++i)
            {
                Entity e = world.createEntity();
                world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
                world.addComponent(e, GhostBlockComponent{});
                ghostBlocks[i] = e;
            }
            updateGhostBlocks(world, grid, active, ghostBlocks);
        }

        // ---------------------------------------------------------------
        // Spawning
        // ---------------------------------------------------------------

        void spawnPiece(ECSWorld& world)
        {
            TetrominoType nextType;

            if (QUEUE_SIZE > 0 && !nextQueue.empty())
            {
                // consume the front of the queue
                nextType = nextQueue.front();
                nextQueue.pop_front();

                // release the front preview slot's entities
                for (int j = 0; j < 4; ++j)
                {
                    world.addComponent(previewBlocks[0][j], RenderResourceClearComponent{});
                    world.removeComponent<TetrisBlockComponent>(previewBlocks[0][j]);
                }
                previewBlocks.erase(previewBlocks.begin());

                // push a new random type onto the back of the queue
                TetrominoType newT = (TetrominoType)(rand() % 7);
                nextQueue.push_back(newT);

                std::array<Entity, 4> newSlot;
                for (int j = 0; j < 4; ++j)
                {
                    Entity e = world.createEntity();
                    world.addComponent(e, TetrisBlockComponent{ 0, 0, true, newT });
                    world.addComponent(e, NextPieceBlockComponent{});
                    newSlot[j] = e;
                }
                previewBlocks.push_back(newSlot);

                updatePreviews(world);
            }
            else
            {
                nextType = (TetrominoType)(rand() % 7);
            }

            active.type     = nextType;
            active.rotation = 0;
            active.pivot    = { grid.width / 2, grid.height - 1 };

            // game over check: spawn position occupied → wipe the board and reset score
            if (!testPosition(grid, active.type, active.pivot, active.rotation))
            {
                auto& sc = world.getSingleton<TetrisScoreComponent>();
                sc.pendingEvents.push_back({ ScoringEventType::GameOver });

                std::vector<Entity> all;
                world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent&)
                {
                    if (!world.hasComponent<NextPieceBlockComponent>(e) &&
                        !world.hasComponent<GhostBlockComponent>(e))
                        all.push_back(e);
                });

                for (Entity e : all)
                {
                    world.addComponent(e, RenderResourceClearComponent{});
                    world.removeComponent<TetrisBlockComponent>(e);
                }

                rebuildGrid(world, grid);
            }

            auto& shape = TetrominoShapes[(int)active.type][0];
            for (int i = 0; i < 4; ++i)
            {
                Entity e = world.createEntity();
                world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
                active.blocks[i] = e;
            }

            applyToActiveBlocks(world);
        }
};
