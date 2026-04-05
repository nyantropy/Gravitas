#pragma once

#include "ECSControllerSystem.hpp"
#include "components/TetrisInputComponent.hpp"
#include "components/TetrisBlockComponent.hpp"
#include "components/GhostBlockComponent.hpp"
#include "components/NextPieceBlockComponent.hpp"
#include "components/HeldPieceBlockComponent.hpp"
#include "systems/TetrisGameSystem.hpp"
#include "GtsKey.h"
#include "ai/AiComponent.h"
#include "ai/AiPlanner.h"

// ECS controller system that drives the Tetris AI.
//
// Execution order: registered immediately after TetrisInputSystem, so it
// runs second in the controller phase each frame.  This lets it suppress the
// player's TetrisInputComponent flags (written by TetrisInputSystem) and
// replace them with AI-generated ones before TetrisGameSystem reads them in
// the next simulation phase.
//
// Move injection model — "hold the flag":
//   The front token's flag is set every frame until TetrisGameSystem has
//   observed and acted on it (detected via piece state change).  The game's
//   own TickTimers debounce is the only throttle; the AI adds none of its own.
//
// Toggle: press T at runtime to enable / disable the AI.
class TetrisAiSystem : public ECSControllerSystem
{
    TetrisGameSystem* gameSystem;

public:
    explicit TetrisAiSystem(TetrisGameSystem& gs) : gameSystem(&gs) {}

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        auto& ai = world.getSingleton<AiComponent>();

        // Toggle AI on / off with T.
        if (ctx.inputSource->isKeyPressed(GtsKey::T))
        {
            ai.enabled    = !ai.enabled;
            ai.moveQueue.clear();
            ai.needsReplan             = ai.enabled;
            ai.lastActivePieceEntityId = UINT32_MAX;

            if (!ai.enabled)
                world.getSingleton<TetrisInputComponent>().clear();
        }

        if (!ai.enabled)
            return;

        // While AI is active, suppress all player input each frame.
        world.getSingleton<TetrisInputComponent>().clear();

        // Read the current piece state once; used for change detection and
        // consumption checks throughout the rest of this function.
        uint32_t currentEntity   = findActivePieceEntity(world);
        int      currentPivotX   = gameSystem->getActivePiecePivotX();
        int      currentPivotY   = gameSystem->getActivePiecePivotY();
        int      currentRotation = gameSystem->getActivePieceRotation();

        // Detect when the active piece changes (new entity IDs on spawn).
        // Handles HARD_DROP and HOLD consumption — when the piece changes,
        // pop a pending HOLD token so it is not re-injected, then replan.
        if (currentEntity != UINT32_MAX && currentEntity != ai.lastActivePieceEntityId)
        {
            ai.lastActivePieceEntityId = currentEntity;
            ai.lastKnownPivotX   = currentPivotX;
            ai.lastKnownPivotY   = currentPivotY;
            ai.lastKnownRotation = currentRotation;

            if (!ai.moveQueue.empty() && ai.moveQueue.front() == MoveToken::HOLD)
                ai.moveQueue.erase(ai.moveQueue.begin());

            // Always replan when piece changes, regardless of why.
            ai.needsReplan = true;
            ai.moveQueue.clear();
        }

        // Build a new move plan whenever a fresh piece appears.
        if (ai.needsReplan && currentEntity != UINT32_MAX)
        {
            replan(ai);
            ai.needsReplan       = false;
            ai.lastKnownPivotX   = currentPivotX;
            ai.lastKnownPivotY   = currentPivotY;
            ai.lastKnownRotation = currentRotation;
        }

        if (ai.moveQueue.empty())
            return;

        MoveToken front = ai.moveQueue.front();

        // Hold the flag set every frame — TetrisGameSystem's internal debounce
        // timer decides when to act; the AI imposes no additional throttle.
        injectMove(world, front);

        // Detect whether the game acted on the front token by observing the
        // change in piece state since the last controller frame.
        // HARD_DROP and HOLD are handled by the entity-change path above and never reach here.
        bool consumed = false;
        switch (front)
        {
            case MoveToken::MOVE_LEFT:
                consumed = (currentPivotX < ai.lastKnownPivotX);
                break;
            case MoveToken::MOVE_RIGHT:
                consumed = (currentPivotX > ai.lastKnownPivotX);
                break;
            case MoveToken::ROTATE_CW:
            case MoveToken::ROTATE_CCW:
                consumed = (currentRotation != ai.lastKnownRotation);
                break;
            case MoveToken::HARD_DROP:
            case MoveToken::HOLD:
                consumed = false;  // handled by entity-change detection above
                break;
        }

        if (consumed)
            ai.moveQueue.erase(ai.moveQueue.begin());

        // Snapshot piece state for next frame's consumption check.
        ai.lastKnownPivotX   = currentPivotX;
        ai.lastKnownPivotY   = currentPivotY;
        ai.lastKnownRotation = currentRotation;
    }

private:
    // Returns the entity ID of the first active-piece block found, or UINT32_MAX.
    // Ghost, next-preview, and held-display blocks are excluded.
    uint32_t findActivePieceEntity(ECSWorld& world) const
    {
        uint32_t result = UINT32_MAX;
        world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& b)
        {
            if (!b.active)                                       return;
            if (world.hasComponent<GhostBlockComponent>(e))     return;
            if (world.hasComponent<NextPieceBlockComponent>(e)) return;
            if (world.hasComponent<HeldPieceBlockComponent>(e)) return;
            if (result == UINT32_MAX)
                result = e.id;
        });
        return result;
    }

    // Build the board snapshot from the authoritative TetrisGrid and choose the
    // best decision (placement or hold) via the lookahead planner.
    void replan(AiComponent& ai) const
    {
        const TetrisGrid& grid = gameSystem->getBoardGrid();
        AiBoard board;
        for (int y = 0; y < grid.height; ++y)
            for (int x = 0; x < grid.width; ++x)
                if (grid.occupied(x, y))
                    board.set(x, y);

        AiPieceState piece;
        piece.type     = static_cast<int>(gameSystem->getActivePieceType());
        piece.rotation = 0;
        piece.pivotX   = ai.lastKnownPivotX;
        piece.pivotY   = ai.lastKnownPivotY;

        int  nextType     = gameSystem->hasNextPiece()
                               ? static_cast<int>(gameSystem->getNextPieceType()) : -1;
        int  nextNextType = gameSystem->hasNextNextPiece()
                               ? static_cast<int>(gameSystem->getNextNextPieceType()) : -1;
        int  heldType     = gameSystem->hasHeldPiece()
                               ? static_cast<int>(gameSystem->getHeldPieceType()) : -1;
        bool holdAvail    = gameSystem->isHoldAvailable();

        AiDecision decision = AiPlanner::ChooseBestDecision(board, piece, nextType,
                                                             heldType, holdAvail, nextNextType);
        if (decision.useHold)
        {
            // Emit HOLD and stop: when the new piece spawns (entity changes),
            // replan() is called again and plans moves for the swapped-in piece.
            ai.moveQueue.push_back(MoveToken::HOLD);
        }
        else
        {
            ai.moveQueue = AiPlanner::PlanMoves(piece, decision.placement);
        }
    }

    void injectMove(ECSWorld& world, MoveToken token) const
    {
        auto& input = world.getSingleton<TetrisInputComponent>();
        switch (token)
        {
            case MoveToken::ROTATE_CW:  input.rotateCW  = true; break;
            case MoveToken::ROTATE_CCW: input.rotateCCW = true; break;
            case MoveToken::MOVE_LEFT:  input.moveLeft  = true; break;
            case MoveToken::MOVE_RIGHT: input.moveRight = true; break;
            case MoveToken::HARD_DROP:  input.hardDrop  = true; break;
            case MoveToken::HOLD:       input.hold      = true; break;
        }
    }
};
