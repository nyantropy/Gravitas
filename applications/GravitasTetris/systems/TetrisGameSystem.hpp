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

        float fallTimer = 0.0f;
        float fallInterval = 0.4f;

        float moveTimer = 0.0f;
        float moveInterval = 0.08f;

        float rotateTimer = 0.0f;
        float rotateInterval = 0.15f;

        std::deque<TetrominoType>           nextQueue;
        std::vector<std::array<Entity, 4>>  previewBlocks;
        std::array<Entity, 4>               ghostBlocks;

        // ---------------------------------------------------------------
        // SRS wall-kick tables (y-UP coordinate system).
        // Each table is indexed by [from_rotation][kick_attempt].
        // Kick attempt 0 is always {0,0} (plain rotation, no offset).
        // ---------------------------------------------------------------
        struct KickOffset { int x; int y; };

        // JLSTZ + O pieces — CW rotation (from_rot → (from_rot+1)%4)
        static constexpr KickOffset KICKS_JLSTZ_CW[4][5] = {{
            { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}   // 0→1
        },{
            { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}   // 1→2
        },{
            { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}   // 2→3
        },{
            { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}   // 3→0
        }};

        // JLSTZ + O pieces — CCW rotation (from_rot → (from_rot-1+4)%4)
        // = negated CW kicks of the target rotation state
        static constexpr KickOffset KICKS_JLSTZ_CCW[4][5] = {{
            { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}   // 0→3
        },{
            { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}   // 1→0
        },{
            { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}   // 2→1
        },{
            { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}   // 3→2
        }};

        // I piece — CW rotation
        static constexpr KickOffset KICKS_I_CW[4][5] = {{
            { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}   // 0→1
        },{
            { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}   // 1→2
        },{
            { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}   // 2→3
        },{
            { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}   // 3→0
        }};

        // I piece — CCW rotation
        static constexpr KickOffset KICKS_I_CCW[4][5] = {{
            { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}   // 0→3
        },{
            { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}   // 1→0
        },{
            { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}   // 2→1
        },{
            { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}   // 3→2
        }};

    public:
        // Number of upcoming pieces shown in the queue. Set to 0 to disable.
        static constexpr int QUEUE_SIZE = 4;

        TetrisGameSystem() {}

        // update tick, this is where all the magic happens
        void update(ECSWorld& world, float dt) override
        {
            rebuildGrid(world);

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
            updateGhost(world);
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
                    tryRotate(world, +1);
                    rotateTimer = 0.0f;
                }
                else if (input.rotateCCW)
                {
                    tryRotate(world, -1);
                    rotateTimer = 0.0f;
                }
            }
        }


        // improved to not hardcap the y coordinate when moving/rotating, increasing the fluidity of gameplay
        bool testPosition(ECSWorld& world, glm::ivec2 newPivot, int newRot) const
        {
            auto& shape = TetrominoShapes[(int)active.type][newRot];

            for (int i = 0; i < 4; ++i)
            {
                int x = newPivot.x + shape.blocks[i].x;
                int y = newPivot.y + shape.blocks[i].y;

                // still clamp left / right / bottom
                if (x < 0 || x >= grid.width || y < 0)
                    return false;

                // only test occupancy if the block is actually inside the grid vertically
                if (y < grid.height && grid.occupied(x, y))
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

        // Attempt to rotate the active piece with SRS wall kicks.
        // direction: +1 = clockwise, -1 = counter-clockwise.
        // Tries up to 5 kick offsets per rotation; applies the first that fits.
        void tryRotate(ECSWorld& world, int direction)
        {
            const int fromRot = active.rotation;
            const int toRot   = (fromRot + direction + 4) % 4;

            // Select kick table for this piece type and direction
            const KickOffset (*kicks)[5] =
                (active.type == TetrominoType::I)
                    ? (direction > 0 ? KICKS_I_CW    : KICKS_I_CCW)
                    : (direction > 0 ? KICKS_JLSTZ_CW : KICKS_JLSTZ_CCW);

            for (int k = 0; k < 5; ++k)
            {
                glm::ivec2 candidate = active.pivot + glm::ivec2(kicks[fromRot][k].x,
                                                                  kicks[fromRot][k].y);
                if (testPosition(world, candidate, toRot))
                {
                    active.pivot    = candidate;
                    active.rotation = toRot;
                    applyToActiveBlocks(world);
                    return;
                }
            }
            // All 5 kick positions blocked — rotation silently fails
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

        // Instantly drop the active piece to its lowest valid position, then lock it.
        // Resets the fall timer so the next piece doesn't immediately drop.
        void hardDrop(ECSWorld& world)
        {
            // Slide the pivot down as far as testPosition allows (same algorithm as ghost)
            while (testPosition(world, { active.pivot.x, active.pivot.y - 1 }, active.rotation))
                active.pivot.y -= 1;

            applyToActiveBlocks(world);

            lockPiece(world);
            rebuildGrid(world);
            clearLines(world);
            rebuildGrid(world);
            spawnPiece(world);

            fallTimer = 0.0f;   // prevent immediate gravity tick on the new piece
        }

        // try to clear lines, only actually deletes anything when a row is full of blocks
        void clearLines(ECSWorld& world)
        {
            int linesCleared = 0;

            for (int y = 0; y < grid.height; ++y)
            {
                bool full = true;
                for (int x = 0; x < grid.width; ++x)
                    if (!grid.occupied(x, y))
                        full = false;

                if (!full) continue;

                linesCleared++;

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
                    world.addComponent(e, RenderResourceClearComponent{});
                    world.removeComponent<TetrisBlockComponent>(e);
                    destroy++;
                }

                // preview blocks have active=true so the !b.active guard already skips them
                world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& b)
                {
                    if (!b.active && b.y > y)
                        b.y -= 1;
                });

                y--;
                rebuildGrid(world);
            }

            if (linesCleared > 0)
            {
                auto& sc = world.getSingleton<TetrisScoreComponent>();
                sc.pendingEvents.push_back({ ScoringEventType::LinesCleared, linesCleared });
            }
        }

        // fill the next-piece queue and create preview entities for each slot
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

        // reposition preview block entities to match the current queue state
        void updatePreviews(ECSWorld& world)
        {
            if (QUEUE_SIZE <= 0) return;

            for (int i = 0; i < (int)previewBlocks.size(); ++i)
            {
                TetrominoType t   = nextQueue[i];
                glm::ivec2 pivot  = { 13, 12 - i * 3 };
                auto& shape       = TetrominoShapes[(int)t][0];

                for (int j = 0; j < 4; ++j)
                {
                    auto& b = world.getComponent<TetrisBlockComponent>(previewBlocks[i][j]);
                    glm::ivec2 p = pivot + shape.blocks[j];
                    b.x = p.x;
                    b.y = p.y;
                }
            }
        }

        // create the 4 persistent ghost block entities; must be called after the first spawnPiece
        void initGhost(ECSWorld& world)
        {
            for (int i = 0; i < 4; ++i)
            {
                Entity e = world.createEntity();
                world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
                world.addComponent(e, GhostBlockComponent{});
                ghostBlocks[i] = e;
            }
            updateGhost(world);
        }

        // cast the active piece straight down to find its landing row, then reposition ghost blocks
        void updateGhost(ECSWorld& world)
        {
            // find the lowest valid pivot by stepping down one row at a time
            glm::ivec2 ghostPivot = active.pivot;
            while (testPosition(world, { ghostPivot.x, ghostPivot.y - 1 }, active.rotation))
                ghostPivot.y -= 1;

            auto& shape = TetrominoShapes[(int)active.type][active.rotation];
            for (int i = 0; i < 4; ++i)
            {
                auto& b      = world.getComponent<TetrisBlockComponent>(ghostBlocks[i]);
                glm::ivec2 p = ghostPivot + shape.blocks[i];
                b.x          = p.x;
                b.y          = p.y;
                b.type       = active.type;   // kept in sync so TetrisVisualSystem can update texture
            }
        }

        // spawn a new tetris piece at the top of the grid (popped from the next queue)
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
            if (!testPosition(world, active.pivot, active.rotation))
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

                rebuildGrid(world);
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
