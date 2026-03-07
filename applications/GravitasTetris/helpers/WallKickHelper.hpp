#pragma once

#include "TetrisPhysicsHelper.hpp"
#include "ActiveTetromino.hpp"
#include "TetrominoType.hpp"
#include <optional>
#include <glm.hpp>

// ---------------------------------------------------------------
// SRS wall-kick tables (y-UP coordinate system).
// Indexed by [from_rotation][kick_attempt].
// Attempt 0 is always {0,0} (plain rotation, no offset).
// ---------------------------------------------------------------
struct KickOffset { int x; int y; };

// JLSTZ + O pieces — CW rotation (from_rot → (from_rot+1)%4)
constexpr KickOffset KICKS_JLSTZ_CW[4][5] = {{
    { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}   // 0→1
},{
    { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}   // 1→2
},{
    { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}   // 2→3
},{
    { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}   // 3→0
}};

// JLSTZ + O pieces — CCW rotation (= negated CW kicks of the target rotation state)
constexpr KickOffset KICKS_JLSTZ_CCW[4][5] = {{
    { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}   // 0→3
},{
    { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}   // 1→0
},{
    { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}   // 2→1
},{
    { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}   // 3→2
}};

// I piece — CW rotation
constexpr KickOffset KICKS_I_CW[4][5] = {{
    { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}   // 0→1
},{
    { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}   // 1→2
},{
    { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}   // 2→3
},{
    { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}   // 3→0
}};

// I piece — CCW rotation
constexpr KickOffset KICKS_I_CCW[4][5] = {{
    { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}   // 0→3
},{
    { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}   // 1→0
},{
    { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}   // 2→1
},{
    { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}   // 3→2
}};

// The resulting pivot and rotation when a kick succeeds.
struct RotationResult
{
    glm::ivec2 pivot;
    int        rotation;
};

// Attempts to rotate the active piece using SRS wall kicks.
// direction: +1 = CW, -1 = CCW.
// Returns the new pivot and rotation on success, nullopt if all 5 kicks fail.
inline std::optional<RotationResult> tryWallKick(
    const TetrisGrid&      grid,
    const ActiveTetromino& active,
    int                    direction)
{
    const int fromRot = active.rotation;
    const int toRot   = (fromRot + direction + 4) % 4;

    const KickOffset (*kicks)[5] =
        (active.type == TetrominoType::I)
            ? (direction > 0 ? KICKS_I_CW    : KICKS_I_CCW)
            : (direction > 0 ? KICKS_JLSTZ_CW : KICKS_JLSTZ_CCW);

    for (int k = 0; k < 5; ++k)
    {
        glm::ivec2 candidate = active.pivot + glm::ivec2(kicks[fromRot][k].x,
                                                          kicks[fromRot][k].y);
        if (testPosition(grid, active.type, candidate, toRot))
            return RotationResult{ candidate, toRot };
    }

    return std::nullopt;
}
