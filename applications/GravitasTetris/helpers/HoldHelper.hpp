#pragma once

#include "TetrominoType.hpp"
#include "TetrominoShape.hpp"
#include "TetrisBlockComponent.hpp"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <array>
#include <glm.hpp>

// Hold slot state.  Owned by TetrisGameSystem as a plain member.
struct HoldState
{
    TetrominoType heldType     = TetrominoType::I;
    bool          hasHeld      = false;   // whether hold slot is occupied
    bool          usedThisTurn = false;   // one-hold-per-piece guard
};

// Return value from tryHold.
struct HoldResult
{
    bool          performed;       // false: blocked (usedThisTurn) — caller does nothing
    bool          hasSwappedIn;    // true: place newActiveType; false: spawn from queue
    TetrominoType newActiveType;   // valid only when hasSwappedIn is true
};

// Validates and performs a hold action.  Updates hold state on success.
//
// Success + hasSwappedIn=true:  the previously-held piece (newActiveType) should become
//   the new active piece with rotation 0 and spawn pivot; caller handles ECS entities.
//
// Success + hasSwappedIn=false: hold slot was empty; caller should spawn the next piece
//   from the queue (spawnPiece covers all queue/entity management in that path).
//
// Failure (performed=false): usedThisTurn was set; caller does nothing.
inline HoldResult tryHold(HoldState& state, TetrominoType activeType)
{
    if (state.usedThisTurn)
        return { false, false, {} };

    state.usedThisTurn = true;

    if (state.hasHeld)
    {
        TetrominoType prev = state.heldType;
        state.heldType     = activeType;
        return { true, true, prev };
    }

    state.heldType = activeType;
    state.hasHeld  = true;
    return { true, false, {} };
}

// Resets the per-piece usage flag.  Must be called whenever a new active piece
// is spawned so hold is available again for the next falling piece.
inline void resetHoldTurn(HoldState& state)
{
    state.usedThisTurn = false;
}

// Fixed screen-space pivot for the held-piece preview (left sidebar).
// Shifted left to ensure no block ever touches or overlaps the playfield border.
// AGENT CHANGE: y moved from 14→16 to vertically align the held piece with the
// first NEXT preview slot (previewPivot(0) = {13, 16}), making both sides
// symmetric: same y start, same block extents (y=15.5..17.5 for standard pieces).
static constexpr glm::ivec2 HOLD_DISPLAY_PIVOT = { -5, 16 };

// Repositions the four persistent hold-display ECS entities to reflect the current
// hold state.  When no piece is held the blocks are moved off-screen (-100, -100).
// Always uses rotation 0, matching modern Tetris convention.
inline void updateHoldDisplay(
    ECSWorld&                     world,
    const HoldState&              state,
    const std::array<Entity, 4>&  holdBlocks)
{
    if (!state.hasHeld)
    {
        for (int i = 0; i < 4; ++i)
        {
            auto& b = world.getComponent<TetrisBlockComponent>(holdBlocks[i]);
            b.x = -100;
            b.y = -100;
        }
        return;
    }

    auto& shape = TetrominoShapes[(int)state.heldType][0];
    for (int i = 0; i < 4; ++i)
    {
        auto& b      = world.getComponent<TetrisBlockComponent>(holdBlocks[i]);
        glm::ivec2 p = HOLD_DISPLAY_PIVOT + shape.blocks[i];
        b.x   = p.x;
        b.y   = p.y;
        b.type = state.heldType;
    }
}
