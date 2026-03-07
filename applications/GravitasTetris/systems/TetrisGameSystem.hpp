#pragma once

#ifdef __INTELLISENSE__
#pragma diag_suppress 70
#pragma diag_suppress 2370
#endif

#include <cstdlib>
#include <glm.hpp>

#include "ECSSimulationSystem.hpp"
#include "TetrisInputComponent.hpp"
#include "TetrisScoreComponent.hpp"
#include "Entity.h"

#include "TetrisTimers.hpp"
#include "TetrisConstants.hpp"
#include "SpeedHelper.hpp"

#include "ActivePieceController.hpp"
#include "GhostController.hpp"
#include "HoldController.hpp"
#include "PieceQueueController.hpp"
#include "BoardController.hpp"
#include "GameStateController.hpp"

// Orchestrates all Tetris game logic each simulation tick.
// All feature state lives in the controllers; this class sequences them.
class TetrisGameSystem : public ECSSimulationSystem
{
    private:
        bool      firstUpdate = true;
        TickTimers timers;

        BoardController       board;
        ActivePieceController piece;
        GhostController       ghost;
        HoldController        hold;
        PieceQueueController  queue;
        GameStateController   gameState;

    public:
        static constexpr int        QUEUE_SIZE         = PieceQueueController::QUEUE_SIZE;
        static constexpr glm::ivec2 NEXT_DISPLAY_PIVOT = PieceQueueController::NEXT_DISPLAY_PIVOT;

        TetrisGameSystem(Entity holdAnchor, Entity nextAnchor)
            : hold(holdAnchor), queue(nextAnchor) {}

        void update(ECSWorld& world, float dt) override
        {
            if (board.isDirty())
                board.rebuild(world);

            if (firstUpdate)
                bootstrap(world);

            timers.advance(dt);
            handleInput(world);
            handleFall(world, dt);
            handleLock(world, dt);

            world.getSingleton<TetrisInputComponent>().clear();
            ghost.sync(world, board.getGrid(), piece.active);
        }

    private:
        void bootstrap(ECSWorld& world)
        {
            queue.init(world);
            spawnPiece(world);
            ghost.init(world, board.getGrid(), piece.active);
            hold.init(world);
            firstUpdate = false;
        }

        // ── input routing ─────────────────────────────────────────────────────
        void handleInput(ECSWorld& world)
        {
            auto& input = world.getSingleton<TetrisInputComponent>();
            if (input.hardDrop) { handleHardDrop(world); return; }
            if (input.hold)     { handleHold(world);     return; }
            handleMovement(world, input);
            handleRotation(world, input);
        }

        void handleMovement(ECSWorld& world, TetrisInputComponent& input)
        {
            if (input.moveLeft  && TickTimers::ready(timers.move, TetrisConstants::MOVE_INTERVAL))
                if (piece.tryMove(world, board.getGrid(), { -1, 0 })) piece.resetLockDelay();
            if (input.moveRight && TickTimers::ready(timers.move, TetrisConstants::MOVE_INTERVAL))
                if (piece.tryMove(world, board.getGrid(), {  1, 0 })) piece.resetLockDelay();
        }

        void handleRotation(ECSWorld& world, TetrisInputComponent& input)
        {
            if (input.rotateCW  && TickTimers::ready(timers.rotate, TetrisConstants::ROTATE_INTERVAL))
                { if (piece.tryRotate(world, board.getGrid(), +1)) piece.resetLockDelay(); }
            else if (input.rotateCCW && TickTimers::ready(timers.rotate, TetrisConstants::ROTATE_INTERVAL))
                { if (piece.tryRotate(world, board.getGrid(), -1)) piece.resetLockDelay(); }
        }

        // ── gravity ───────────────────────────────────────────────────────────
        void handleFall(ECSWorld& world, float dt)
        {
            auto& input = world.getSingleton<TetrisInputComponent>();
            const float baseFall = SpeedHelper::computeSpeedLevel(
                world.getSingleton<TetrisScoreComponent>().score).fallInterval;
            const float interval = input.softDrop ? TetrisConstants::SOFTDROP_INTERVAL : baseFall;
            if (TickTimers::ready(timers.fall, interval))
                if (piece.tryMove(world, board.getGrid(), { 0, -1 })) piece.resetLockDelay();
        }

        // ── lock delay ────────────────────────────────────────────────────────
        void handleLock(ECSWorld& world, float dt)
        {
            auto& input = world.getSingleton<TetrisInputComponent>();
            if (!piece.isGrounded(board.getGrid())) { piece.resetLockDelay(); return; }
            if (input.softDrop)                      { doLock(world); return; }
            piece.addLockDelay(dt);
            if (piece.lockExpired(TetrisConstants::LOCK_DELAY_LIMIT)) doLock(world);
        }

        // ── actions ───────────────────────────────────────────────────────────
        void handleHardDrop(ECSWorld& world)
        {
            piece.hardDropSnap(world, board.getGrid());
            doLock(world);
            timers.reset();
        }

        void handleHold(ECSWorld& world)
        {
            auto result = hold.tryHold(piece.active.type);
            if (!result.performed) return;

            piece.clearFromECS(world);

            if (result.hasSwappedIn)
            {
                piece.prepareSpawn(board.getGrid(), result.newActiveType);
                if (piece.isGrounded(board.getGrid()))
                    gameState.triggerGameOver(world, board);
                piece.materializeBlocks(world);
                board.markDirty();
            }
            else
            {
                spawnPiece(world);
            }

            timers.reset();
            hold.refreshDisplay(world);
        }

        void doLock(ECSWorld& world)
        {
            piece.lockPiece(world);
            board.rebuild(world);
            board.clearLines(world);
            board.rebuild(world);
            spawnPiece(world);
        }

        // Consumes the next piece from the queue, prepares the active piece,
        // checks game over, materializes entities, and marks the board dirty.
        void spawnPiece(ECSWorld& world)
        {
            hold.resetTurn();

            TetrominoType type = queue.hasQueue()
                ? queue.advance(world)
                : (TetrominoType)(rand() % 7);

            piece.prepareSpawn(board.getGrid(), type);
            if (piece.isGrounded(board.getGrid()))
                gameState.triggerGameOver(world, board);
            piece.materializeBlocks(world);
            board.markDirty();
        }
};
