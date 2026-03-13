#include "AiPlanner.h"
#include "TetrominoShape.hpp"

#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// Pure physics helpers — use TetrominoShapes[type][rotation].blocks[i].x/y
// ---------------------------------------------------------------------------

static bool testPosition(const AiBoard& board, int type, int rotation, int pivotX, int pivotY)
{
    for (int i = 0; i < 4; ++i)
    {
        int x = pivotX + TetrominoShapes[type][rotation].blocks[i].x;
        int y = pivotY + TetrominoShapes[type][rotation].blocks[i].y;
        if (x < 0 || x >= AiBoard::W || y < 0) return false;
        if (y < AiBoard::H && board.occupied(x, y)) return false;
    }
    return true;
}

static int computeDropPivotY(const AiBoard& board, int type, int rotation, int pivotX, int startY)
{
    int y = startY;
    while (testPosition(board, type, rotation, pivotX, y - 1))
        y -= 1;
    return y;
}

static AiBoard lockPieceOnBoard(const AiBoard& board, int type, int rotation, int pivotX, int pivotY)
{
    AiBoard result = board;
    for (int i = 0; i < 4; ++i)
    {
        int x = pivotX + TetrominoShapes[type][rotation].blocks[i].x;
        int y = pivotY + TetrominoShapes[type][rotation].blocks[i].y;
        if (x >= 0 && x < AiBoard::W && y >= 0 && y < AiBoard::H)
            result.set(x, y);
    }
    return result;
}

// Returns the number of lines cleared and modifies the board in place.
static int clearLines(AiBoard& board)
{
    int cleared = 0;
    for (int y = 0; y < AiBoard::H;)
    {
        bool full = true;
        for (int x = 0; x < AiBoard::W; ++x)
            if (!board.cells[y][x]) { full = false; break; }

        if (!full) { ++y; continue; }

        for (int row = y; row < AiBoard::H - 1; ++row)
            for (int x = 0; x < AiBoard::W; ++x)
                board.cells[row][x] = board.cells[row + 1][x];

        for (int x = 0; x < AiBoard::W; ++x)
            board.cells[AiBoard::H - 1][x] = false;

        ++cleared;
    }
    return cleared;
}

// ---------------------------------------------------------------------------
// Scoring heuristic
// ---------------------------------------------------------------------------

// Rewards a right-column well (col W-1 noticeably lower than col W-2) for
// Tetris setups; penalises accidental wells elsewhere.
static float ComputeWellPenalty(const int heights[AiBoard::W])
{
    float penalty = 0.0f;

    // Reward right-column well of depth > 3 (Tetris setup).
    if (heights[AiBoard::W - 2] - heights[AiBoard::W - 1] > 3)
        penalty -= 4.0f;

    // Penalise wells in columns 0..W-2.
    for (int x = 0; x < AiBoard::W - 1; ++x)
    {
        int left  = (x > 0)              ? heights[x - 1] : AiBoard::H;
        int right = (x < AiBoard::W - 1) ? heights[x + 1] : AiBoard::H;
        if (std::min(left, right) - heights[x] > 2)
            penalty += 2.0f;
    }

    return penalty;
}

static float ScoreBoard(const AiBoard& board, int linesCleared)
{
    int heights[AiBoard::W] = {};
    int aggregateHeight     = 0;
    int holes               = 0;
    int bumpiness           = 0;

    for (int x = 0; x < AiBoard::W; ++x)
    {
        for (int y = AiBoard::H - 1; y >= 0; --y)
            if (board.cells[y][x]) { heights[x] = y + 1; break; }
        aggregateHeight += heights[x];
    }

    for (int x = 0; x < AiBoard::W; ++x)
    {
        bool seenBlock = false;
        for (int y = AiBoard::H - 1; y >= 0; --y)
        {
            if (board.cells[y][x])  seenBlock = true;
            else if (seenBlock)     ++holes;
        }
    }

    for (int x = 0; x < AiBoard::W - 1; ++x)
        bumpiness += std::abs(heights[x] - heights[x + 1]);

    // Exponential line-clear bonus: 0/1/3/8/20 for 0/1/2/3/4 lines.
    // Tetris capped at 20 so the AI won't sacrifice survival to chase one.
    static const float lineClearBonus[] = { 0.0f, 1.0f, 3.0f, 8.0f, 20.0f };
    float lineScore = lineClearBonus[std::min(linesCleared, 4)];

    static constexpr float WEIGHT_HEIGHT    = 2.5f;
    static constexpr float WEIGHT_HOLES     = 10.0f;
    static constexpr float WEIGHT_BUMPINESS = 3.0f;

    return lineScore
         - float(aggregateHeight) * WEIGHT_HEIGHT
         - float(holes)           * WEIGHT_HOLES
         - float(bumpiness)       * WEIGHT_BUMPINESS
         + ComputeWellPenalty(heights);
}

// ---------------------------------------------------------------------------
// Internal: enumerate all valid placements with their locked boards.
// Shared by all planning functions.
// ---------------------------------------------------------------------------

struct PlacementResult
{
    int     rotation, pivotX, pivotY;
    float   singleScore;
    AiBoard boardAfter;
};

static std::vector<PlacementResult> generatePlacements(const AiBoard& board,
                                                        const AiPieceState& piece)
{
    std::vector<PlacementResult> results;

    for (int rot = 0; rot < 4; ++rot)
    {
        for (int px = -4; px <= AiBoard::W + 3; ++px)
        {
            int dropY = computeDropPivotY(board, piece.type, rot, px, piece.pivotY);

            if (!testPosition(board, piece.type, rot, px, dropY))
                continue;

            bool inGrid = false;
            for (int i = 0; i < 4; ++i)
            {
                int by = dropY + TetrominoShapes[piece.type][rot].blocks[i].y;
                if (by >= 0 && by < AiBoard::H) { inGrid = true; break; }
            }
            if (!inGrid) continue;

            AiBoard afterLock = lockPieceOnBoard(board, piece.type, rot, px, dropY);
            int     lines     = clearLines(afterLock);
            float   score     = ScoreBoard(afterLock, lines);

            results.push_back({ rot, px, dropY, score, afterLock });
        }
    }

    return results;
}

// ---------------------------------------------------------------------------
// Recursive lookahead scorer
//
// Returns the best achievable combined score from placing pieceType on board,
// then looking depth levels deeper using nextPieceTypes[0..nextCount-1].
//
// depth == 0: return ScoreBoard(board, 0) — evaluate without placing.
// depth  > 0: enumerate placements of pieceType, for each lock+clear recurse
//             with depth-1; score = ScoreBoard(after, lines) + 0.4 * child.
// ---------------------------------------------------------------------------
static float ScoreWithLookahead(const AiBoard& board, int pieceType, int depth,
                                 const int* nextPieceTypes, int nextCount)
{
    if (depth == 0)
        return ScoreBoard(board, 0);

    AiPieceState piece{ pieceType, 0, 5, 20 };
    auto candidates = generatePlacements(board, piece);

    if (candidates.empty())
        return ScoreBoard(board, 0);

    float best = std::numeric_limits<float>::lowest();
    for (const auto& cand : candidates)
    {
        float childScore;
        if (nextCount > 0)
            childScore = ScoreWithLookahead(cand.boardAfter, nextPieceTypes[0],
                                            depth - 1, nextPieceTypes + 1, nextCount - 1);
        else
            childScore = ScoreBoard(cand.boardAfter, 0);

        float score = cand.singleScore + 0.4f * childScore;
        if (score > best) best = score;
    }
    return best;
}

// ---------------------------------------------------------------------------
// Public AiPlanner API
// ---------------------------------------------------------------------------

AiPlacement AiPlanner::ChoosePlacement(const AiBoard& board, const AiPieceState& piece)
{
    auto candidates = generatePlacements(board, piece);

    if (candidates.empty())
        return { piece.rotation, piece.pivotX, piece.pivotY, 0.0f };

    auto it = std::max_element(candidates.begin(), candidates.end(),
        [](const PlacementResult& a, const PlacementResult& b)
        { return a.singleScore < b.singleScore; });

    return { it->rotation, it->pivotX, it->pivotY, it->singleScore };
}

AiPlacement AiPlanner::ChoosePlacement(const AiBoard& board, const AiPieceState& piece,
                                        int nextPieceType)
{
    auto candidates = generatePlacements(board, piece);

    if (candidates.empty())
        return { piece.rotation, piece.pivotX, piece.pivotY, 0.0f };

    AiPieceState nextPiece{ nextPieceType, 0, 5, 20 };

    float                  bestCombined = std::numeric_limits<float>::lowest();
    const PlacementResult* bestCand     = nullptr;

    for (const auto& cand : candidates)
    {
        AiPlacement lookahead = ChoosePlacement(cand.boardAfter, nextPiece);
        float       combined  = cand.singleScore + lookahead.score * 0.5f;

        if (combined > bestCombined)
        {
            bestCombined = combined;
            bestCand     = &cand;
        }
    }

    return { bestCand->rotation, bestCand->pivotX, bestCand->pivotY, bestCombined };
}

AiDecision AiPlanner::ChooseBestDecision(const AiBoard& board, const AiPieceState& current,
                                          int nextPieceType, int heldPieceType,
                                          bool holdAvailable, int nextNextPieceType)
{
    auto currentCandidates = generatePlacements(board, current);
    if (currentCandidates.empty())
        return { { current.rotation, current.pivotX, current.pivotY, 0.0f }, false };

    // ── No-hold branch ────────────────────────────────────────────────────
    // Place current piece; lookahead: next, then next-next.
    float                  bestDirectScore = std::numeric_limits<float>::lowest();
    const PlacementResult* bestDirectCand  = nullptr;

    for (const auto& cand : currentCandidates)
    {
        float childScore = 0.0f;
        if (nextPieceType >= 0)
        {
            const int remaining[] = { nextNextPieceType };
            childScore = ScoreWithLookahead(cand.boardAfter, nextPieceType, 1,
                                            remaining, (nextNextPieceType >= 0) ? 1 : 0);
        }
        float combined = cand.singleScore + 0.4f * childScore;
        if (combined > bestDirectScore)
        {
            bestDirectScore = combined;
            bestDirectCand  = &cand;
        }
    }

    AiPlacement directPlacement = {
        bestDirectCand->rotation, bestDirectCand->pivotX, bestDirectCand->pivotY,
        bestDirectScore
    };

    if (!holdAvailable)
        return { directPlacement, false };

    // ── Hold branch A: slot occupied — swap held piece in ─────────────────
    // Place held piece; after hold: current.type goes to the hold slot.
    // Lookahead: current.type (now in hold, comes back next), then next from queue.
    if (heldPieceType >= 0)
    {
        AiPieceState heldState{ heldPieceType, 0, 5, 20 };
        auto heldCandidates = generatePlacements(board, heldState);

        float                  bestHoldScore = std::numeric_limits<float>::lowest();
        const PlacementResult* bestHoldCand  = nullptr;

        for (const auto& cand : heldCandidates)
        {
            // current.type is the immediate next piece (went to hold); nextPieceType
            // follows after that from the queue.
            const int remaining[] = { nextPieceType };
            float childScore = ScoreWithLookahead(cand.boardAfter, current.type, 1,
                                                  remaining, (nextPieceType >= 0) ? 1 : 0);
            float combined = cand.singleScore + 0.4f * childScore;
            if (combined > bestHoldScore)
            {
                bestHoldScore = combined;
                bestHoldCand  = &cand;
            }
        }

        if (bestHoldCand && bestHoldScore > bestDirectScore)
        {
            AiPlacement heldPlacement = {
                bestHoldCand->rotation, bestHoldCand->pivotX, bestHoldCand->pivotY,
                bestHoldScore
            };
            return { heldPlacement, true };
        }

        return { directPlacement, false };
    }

    // ── Hold branch B: slot empty — cycle: current → hold, next → active ──
    // Only evaluate if a next piece actually exists.
    if (nextPieceType >= 0)
    {
        AiPieceState nextAsActive{ nextPieceType, 0, 5, 20 };
        auto nextCandidates = generatePlacements(board, nextAsActive);

        float                  bestEmptyHoldScore = std::numeric_limits<float>::lowest();
        const PlacementResult* bestEmptyHoldCand  = nullptr;

        for (const auto& cand : nextCandidates)
        {
            // After placing nextPieceType, the sequence is:
            //   current.type (in hold, comes back next), then nextNextPieceType.
            const int remaining[] = { nextNextPieceType };
            float childScore = ScoreWithLookahead(
                cand.boardAfter, current.type, 1,
                remaining, (nextNextPieceType >= 0) ? 1 : 0);
            float combined = cand.singleScore + 0.4f * childScore;
            if (combined > bestEmptyHoldScore)
            {
                bestEmptyHoldScore = combined;
                bestEmptyHoldCand  = &cand;
            }
        }

        if (bestEmptyHoldCand && bestEmptyHoldScore > bestDirectScore)
        {
            AiPlacement emptyHoldPlacement = {
                bestEmptyHoldCand->rotation,
                bestEmptyHoldCand->pivotX,
                bestEmptyHoldCand->pivotY,
                bestEmptyHoldScore
            };
            return { emptyHoldPlacement, true };
        }
    }

    return { directPlacement, false };
}

std::vector<MoveToken> AiPlanner::PlanMoves(const AiPieceState& current, const AiPlacement& target)
{
    std::vector<MoveToken> moves;

    // Rotations: 1 step CW, 2 steps CW, or 1 step CCW (shortest path).
    int deltaRot = (target.rotation - current.rotation + 4) % 4;
    if (deltaRot == 1)
    {
        moves.push_back(MoveToken::ROTATE_CW);
    }
    else if (deltaRot == 2)
    {
        moves.push_back(MoveToken::ROTATE_CW);
        moves.push_back(MoveToken::ROTATE_CW);
    }
    else if (deltaRot == 3)
    {
        moves.push_back(MoveToken::ROTATE_CCW);
    }

    // Lateral translation.
    int       deltaX = target.pivotX - current.pivotX;
    MoveToken dir    = (deltaX < 0) ? MoveToken::MOVE_LEFT : MoveToken::MOVE_RIGHT;
    for (int i = 0; i < std::abs(deltaX); ++i)
        moves.push_back(dir);

    // Hard drop to lock.
    moves.push_back(MoveToken::HARD_DROP);

    return moves;
}
