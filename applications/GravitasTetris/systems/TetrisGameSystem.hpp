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

#include "HeldPieceBlockComponent.hpp"

#include "TetrisTimers.hpp"
#include "TetrisPhysicsHelper.hpp"
#include "WallKickHelper.hpp"
#include "GhostHelper.hpp"
#include "LineClearHelper.hpp"
#include "PreviewQueueHelper.hpp"
#include "HoldHelper.hpp"
#include "SpeedHelper.hpp"
#include "HierarchyHelper.h"

// Orchestrates all Tetris game logic each simulation tick.
// Feature logic is fully delegated to the helpers above; this class owns
// only ECS state (active piece, grid cache, queue, timers) and sequences them.
class TetrisGameSystem : public ECSSimulationSystem
{
    private:
        bool            firstUpdate = true;
        TetrisGrid      grid;
        ActiveTetromino active;
        TickTimers      timers;

        // Timing constants
        // Fall interval is now dynamic (speed level from SpeedHelper); only the
        // soft-drop and input debounce intervals are fixed.
        static constexpr float SOFTDROP_INTERVAL = 0.05f;
        static constexpr float MOVE_INTERVAL     = 0.08f;
        static constexpr float ROTATE_INTERVAL   = 0.15f;
        static constexpr float LOCK_DELAY        = 0.5f;

        // Lock delay: accumulated while the piece rests on the ground.
        // Resets to zero on any successful lateral move, rotation, or downward step.
        // When it reaches LOCK_DELAY the piece is locked in place.
        float lockDelay = 0.0f;

        // Dirty flag: set true whenever locked blocks or cleared rows change the grid.
        // Only then is the grid cache rebuilt at the start of the next frame.
        bool gridDirty = true;

        std::deque<TetrominoType>           nextQueue;
        std::vector<std::array<Entity, 4>>  previewBlocks;
        std::array<Entity, 4>               ghostBlocks;

        HoldState             holdState;
        std::array<Entity, 4> holdDisplayBlocks;

        // Anchors for the Hold and Next UI groups (set from TetrisScene at construction).
        Entity holdAnchor;
        Entity nextAnchor;

    public:
        // Number of upcoming pieces shown in the queue.  Set to 0 to disable.
        static constexpr int QUEUE_SIZE = 4;

        // World-space position of the Next UI anchor (first preview slot origin).
        // Used by TetrisScene to place the nextGroupAnchor entity.
        static constexpr glm::ivec2 NEXT_DISPLAY_PIVOT = { 13, 16 };

        TetrisGameSystem(Entity holdAnchor, Entity nextAnchor)
            : holdAnchor(holdAnchor), nextAnchor(nextAnchor) {}

        void update(ECSWorld& world, float dt) override
        {
            // Grid is rebuilt only when a mutation (lock / clear / spawn) has marked it dirty.
            if (gridDirty)
            {
                rebuildGrid(world, grid);
                gridDirty = false;
            }

            if (firstUpdate)
            {
                initQueue(world);
                spawnPiece(world);
                initGhost(world);        // must follow spawnPiece so active.type is set
                initHoldDisplay(world);  // persistent hold entities; invisible until first hold
                firstUpdate = false;
            }

            timers.advance(dt);
            handleInput(world);

            // Gravity: try to fall one row; reset lock delay on success.
            // Fall speed scales with the current score via SpeedHelper.
            auto& input = world.getSingleton<TetrisInputComponent>();
            const float baseFall   = computeSpeedLevel(world.getSingleton<TetrisScoreComponent>().score).fallInterval;
            float fallInterval = input.softDrop ? SOFTDROP_INTERVAL : baseFall;
            if (TickTimers::ready(timers.fall, fallInterval))
            {
                if (tryMove(world, { 0, -1 }))
                    lockDelay = 0.0f;
            }

            // Lock delay: accumulate while grounded; lock when the window expires.
            // Soft drop bypasses the window entirely — holding S locks the piece
            // on the first frame it touches the ground, matching the fast-drop feel.
            // If the piece moves off the ground (e.g. after a rotation kick) the
            // accumulator is reset so the full window restarts.
            const bool grounded = !testPosition(
                grid, active.type,
                { active.pivot.x, active.pivot.y - 1 },
                active.rotation);

            if (grounded)
            {
                if (input.softDrop)
                {
                    doLock(world);
                }
                else
                {
                    lockDelay += dt;
                    if (lockDelay >= LOCK_DELAY)
                        doLock(world);
                }
            }
            else
            {
                lockDelay = 0.0f;
            }

            input.clear();
            updateGhostBlocks(world, grid, active, ghostBlocks);
        }


    private:
        void handleInput(ECSWorld& world)
        {
            auto& input = world.getSingleton<TetrisInputComponent>();

            // Hard drop: bypasses lock delay and locks immediately.
            if (input.hardDrop)
            {
                hardDrop(world);
                return;
            }

            // Hold: store current piece; swap if hold slot is occupied.
            // Single-shot (isActionPressed), so return early to avoid processing
            // other inputs on the same frame as the piece swap.
            if (input.hold)
            {
                doHold(world);
                return;
            }

            // Lateral movement: reset lock delay on any successful shift.
            if (input.moveLeft && TickTimers::ready(timers.move, MOVE_INTERVAL))
            {
                if (tryMove(world, { -1, 0 }))
                    lockDelay = 0.0f;
            }

            if (input.moveRight && TickTimers::ready(timers.move, MOVE_INTERVAL))
            {
                if (tryMove(world, { 1, 0 }))
                    lockDelay = 0.0f;
            }

            // Rotation: uses TickTimers::ready for API consistency with movement.
            // CW takes precedence when both are held.  The timer only resets when
            // an input is actually present — idle frames leave it accumulated so the
            // next keypress fires immediately (same feel as the raw-compare approach).
            // A successful rotation also resets the lock delay.
            if (input.rotateCW && TickTimers::ready(timers.rotate, ROTATE_INTERVAL))
            {
                if (auto result = tryWallKick(grid, active, +1))
                {
                    active.pivot    = result->pivot;
                    active.rotation = result->rotation;
                    applyToActiveBlocks(world);
                    lockDelay = 0.0f;
                }
            }
            else if (input.rotateCCW && TickTimers::ready(timers.rotate, ROTATE_INTERVAL))
            {
                if (auto result = tryWallKick(grid, active, -1))
                {
                    active.pivot    = result->pivot;
                    active.rotation = result->rotation;
                    applyToActiveBlocks(world);
                    lockDelay = 0.0f;
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

        // Attempts to move the active piece by delta.
        // Returns true on success (pivot updated, blocks repositioned).
        // Never locks — locking is handled by doLock() via the lock delay system.
        bool tryMove(ECSWorld& world, glm::ivec2 delta)
        {
            glm::ivec2 newPivot = active.pivot + delta;
            if (testPosition(grid, active.type, newPivot, active.rotation))
            {
                active.pivot = newPivot;
                applyToActiveBlocks(world);
                return true;
            }
            return false;
        }

        void lockPiece(ECSWorld& world)
        {
            for (int i = 0; i < 4; ++i)
                world.getComponent<TetrisBlockComponent>(active.blocks[i]).active = false;
            gridDirty = true;   // locked blocks now part of the grid state
        }

        // Locks the active piece, clears lines, and spawns the next piece.
        // Called by the lock delay system (normal lock) and hardDrop.
        void doLock(ECSWorld& world)
        {
            lockPiece(world);
            rebuildGrid(world, grid);   // must be current before clearLines scans rows
            clearLines(world, grid);    // clears full rows; internal rebuilds per row
            rebuildGrid(world, grid);   // final sync after all row removals
            spawnPiece(world);          // resets lockDelay and marks gridDirty
        }

        // Snaps the piece to its lowest valid row immediately, then delegates to doLock.
        // Resets buffered move/rotate timers so the new piece cannot inherit held-key state.
        void hardDrop(ECSWorld& world)
        {
            active.pivot = computeDropPivot(grid, active);
            applyToActiveBlocks(world);

            doLock(world);

            timers.fall   = 0.0f;   // prevent immediate gravity tick on new piece
            timers.move   = 0.0f;   // clear buffered lateral movement
            timers.rotate = 0.0f;   // clear buffered rotation
        }

        // Returns the LOCAL pivot for preview slot i, relative to nextAnchor at NEXT_DISPLAY_PIVOT.
        // Slot 0 is at the anchor origin; each subsequent slot is 3 units lower.
        static glm::ivec2 previewPivot(int i) { return { 0, -i * 3 }; }

        void initQueue(ECSWorld& world)
        {
            if (QUEUE_SIZE <= 0) return;

            nextQueue.clear();
            previewBlocks.resize(QUEUE_SIZE);

            for (int i = 0; i < QUEUE_SIZE; ++i)
            {
                TetrominoType t = (TetrominoType)(rand() % 7);
                nextQueue.push_back(t);
                previewBlocks[i] = spawnPreviewPiece(world, t, previewPivot(i));
                for (int j = 0; j < 4; ++j)
                    setParent(world, previewBlocks[i][j], nextAnchor);
            }
            // spawnPreviewPiece already positions blocks; no updatePreviews needed here
        }

        // Repositions all existing preview entities to match the current queue order.
        // Called each time the queue shifts so all slots slide up by one.
        void updatePreviews(ECSWorld& world)
        {
            if (QUEUE_SIZE <= 0) return;

            for (int i = 0; i < (int)previewBlocks.size(); ++i)
            {
                TetrominoType t  = nextQueue[i];
                glm::ivec2 pivot = previewPivot(i);
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

        bool isGameOver() const
        {
            glm::ivec2 entryPivot = { active.pivot.x, active.pivot.y - 1 };
            return !testPosition(grid, active.type, entryPivot, active.rotation);
        }

        // Creates the four persistent held-piece display entities; initially hidden off-screen.
        // Each block is parented to holdAnchor so the whole group moves with that anchor.
        void initHoldDisplay(ECSWorld& world)
        {
            for (int i = 0; i < 4; ++i)
            {
                Entity e = world.createEntity();
                // Initial position is local-space off-screen; anchor at HOLD_DISPLAY_PIVOT
                // places world position at approximately (-105, -84) — safely invisible.
                world.addComponent(e, TetrisBlockComponent{ -100, -100, true, TetrominoType::I });
                world.addComponent(e, HeldPieceBlockComponent{});
                holdDisplayBlocks[i] = e;
                setParent(world, e, holdAnchor);
            }
        }

        // Attempts to hold the current active piece.
        // On first hold: stores the piece and spawns the next from the queue.
        // On subsequent holds: swaps the active piece with the stored one.
        // Silently ignored when hold was already used this turn.
        void doHold(ECSWorld& world)
        {
            HoldResult result = tryHold(holdState, active.type);
            if (!result.performed) return;

            // Remove current active block entities — the piece is being held, not locked.
            for (int i = 0; i < 4; ++i)
            {
                world.addComponent(active.blocks[i], RenderResourceClearComponent{});
                world.removeComponent<TetrisBlockComponent>(active.blocks[i]);
            }

            if (result.hasSwappedIn)
            {
                // Place the previously-held piece as the new active piece.
                // Rotation resets to 0 and pivot resets to the standard spawn position.
                active.type     = result.newActiveType;
                active.rotation = 0;
                active.pivot    = { grid.width / 2, grid.height };

                if (isGameOver())
                {
                    auto& sc = world.getSingleton<TetrisScoreComponent>();
                    sc.pendingEvents.push_back({ ScoringEventType::GameOver });

                    std::vector<Entity> toWipe;
                    world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent&)
                    {
                        if (!world.hasComponent<NextPieceBlockComponent>(e) &&
                            !world.hasComponent<GhostBlockComponent>(e)    &&
                            !world.hasComponent<HeldPieceBlockComponent>(e))
                            toWipe.push_back(e);
                    });
                    for (Entity e : toWipe)
                    {
                        world.addComponent(e, RenderResourceClearComponent{});
                        world.removeComponent<TetrisBlockComponent>(e);
                    }
                    rebuildGrid(world, grid);
                }

                for (int i = 0; i < 4; ++i)
                {
                    Entity e = world.createEntity();
                    world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
                    active.blocks[i] = e;
                }
                applyToActiveBlocks(world);

                gridDirty = true;
                lockDelay = 0.0f;
            }
            else
            {
                // Hold slot was empty: spawn the next piece from the queue.
                // spawnPiece handles queue management, entity creation, and game-over check.
                spawnPiece(world);
            }

            timers.fall   = 0.0f;
            timers.move   = 0.0f;
            timers.rotate = 0.0f;
            updateHoldDisplay(world, holdState, holdDisplayBlocks);
        }

        void spawnPiece(ECSWorld& world)
        {
            resetHoldTurn(holdState);   // new piece: hold is available again

            TetrominoType nextType;

            if (QUEUE_SIZE > 0 && !nextQueue.empty())
            {
                // Consume the front of the queue
                nextType = nextQueue.front();
                nextQueue.pop_front();

                // Release the front preview slot's entities
                for (int j = 0; j < 4; ++j)
                {
                    world.addComponent(previewBlocks[0][j], RenderResourceClearComponent{});
                    world.removeComponent<TetrisBlockComponent>(previewBlocks[0][j]);
                }
                previewBlocks.erase(previewBlocks.begin());

                // Push a new random type onto the back of the queue
                TetrominoType newT = (TetrominoType)(rand() % 7);
                nextQueue.push_back(newT);
                previewBlocks.push_back(spawnPreviewPiece(world, newT, previewPivot(QUEUE_SIZE - 1)));
                for (int j = 0; j < 4; ++j)
                    setParent(world, previewBlocks.back()[j], nextAnchor);

                updatePreviews(world);
            }
            else
            {
                nextType = (TetrominoType)(rand() % 7);
            }

            active.type     = nextType;
            active.rotation = 0;
            // Spawn one row above the visible grid so pieces can rotate freely at the top
            // without being artificially clipped. testPosition allows y >= grid.height.
            active.pivot    = { grid.width / 2, grid.height };

            // Game over: the piece cannot enter the visible playfield.
            if (isGameOver())
            {
                auto& sc = world.getSingleton<TetrisScoreComponent>();
                sc.pendingEvents.push_back({ ScoringEventType::GameOver });

                std::vector<Entity> all;
                world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent&)
                {
                    if (!world.hasComponent<NextPieceBlockComponent>(e) &&
                        !world.hasComponent<GhostBlockComponent>(e)    &&
                        !world.hasComponent<HeldPieceBlockComponent>(e))
                        all.push_back(e);
                });

                for (Entity e : all)
                {
                    world.addComponent(e, RenderResourceClearComponent{});
                    world.removeComponent<TetrisBlockComponent>(e);
                }

                rebuildGrid(world, grid);   // wipe is an immediate mutation
            }

            for (int i = 0; i < 4; ++i)
            {
                Entity e = world.createEntity();
                world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
                active.blocks[i] = e;
            }

            applyToActiveBlocks(world);

            // New active blocks change visible state; mark the grid dirty and ensure
            // the lock delay window starts clean for the freshly spawned piece.
            gridDirty = true;
            lockDelay = 0.0f;
        }
};
