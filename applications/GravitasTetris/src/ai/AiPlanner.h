#pragma once

// Pure planning logic — zero ECS or engine includes.
// All inputs and outputs are plain data types from AiTypes.h.

#include "ai/AiTypes.h"
#include <vector>

// State of the active falling piece at the moment planning begins.
// The AI always plans from the spawn state (rotation 0, spawn pivot).
struct AiPieceState
{
    int type;      // 0-6, same ordinal as TetrominoType enum
    int rotation;  // 0-3
    int pivotX;
    int pivotY;
};

// A candidate placement: the piece locked at (rotation, pivotX, pivotY).
struct AiPlacement
{
    int   rotation;
    int   pivotX;
    int   pivotY;
    float score;
};

// The result of the top-level decision: which placement to use, and whether to
// spend the hold action first (swap held↔active before placing).
struct AiDecision
{
    AiPlacement placement;
    bool        useHold = false;
};

namespace AiPlanner
{
    // Choose the best placement for the given piece on the given board.
    AiPlacement            ChoosePlacement(const AiBoard& board, const AiPieceState& piece);

    // Lookahead overload: after locking the current piece, scores each candidate by
    // also running ChoosePlacement on the resulting board with the next piece.
    // Combined score = currentScore + lookaheadBestScore * 0.5f.
    AiPlacement            ChoosePlacement(const AiBoard& board, const AiPieceState& piece,
                                           int nextPieceType);

    // Top-level decision: compares placing the current piece directly against
    // swapping it into hold and playing the held piece instead.
    // Uses 2-piece lookahead when nextNextPieceType >= 0.
    // heldPieceType < 0 means the hold slot is empty (hold is never chosen then).
    // holdAvailable must be true for hold to be considered.
    AiDecision             ChooseBestDecision(const AiBoard& board, const AiPieceState& current,
                                              int nextPieceType, int heldPieceType,
                                              bool holdAvailable, int nextNextPieceType);

    // Generate the ordered move sequence from current state to target placement.
    std::vector<MoveToken> PlanMoves(const AiPieceState& current, const AiPlacement& target);
}
