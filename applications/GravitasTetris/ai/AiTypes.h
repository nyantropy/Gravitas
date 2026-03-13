#pragma once

#include <cstdint>

// Plain data types shared between AiPlanner and the ECS layer.
// No engine or ECS headers are included here.

enum class MoveToken { ROTATE_CW, ROTATE_CCW, MOVE_LEFT, MOVE_RIGHT, HARD_DROP, HOLD };

struct AiBoard
{
    static constexpr int W = 10;
    static constexpr int H = 20;

    bool cells[H][W] = {};

    bool occupied(int x, int y) const { return cells[y][x]; }
    void set(int x, int y)            { cells[y][x] = true; }
};
