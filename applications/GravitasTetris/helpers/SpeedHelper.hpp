#pragma once

#include <cmath>
#include <algorithm>

// Speed-level mapping.  Shared by TetrisGameSystem (fall interval) and
// TetrisScoreSystem (level display).
namespace SpeedHelper
{
    struct SpeedLevel
    {
        int   level;        // 1-based (1 = slowest, MAX_LEVEL = fastest)
        float fallInterval; // seconds per automatic gravity tick
    };

    // Maps current score to a speed level and fall interval.
    //
    // level    = clamp(score / POINTS_PER_LEVEL + 1, 1, MAX_LEVEL)
    // interval = START * pow(END / START, (level-1)/(MAX_LEVEL-1))
    //
    // Level 1 → 0.80 s/row  |  Level 8 → ~0.28 s/row  |  Level 15 → 0.10 s/row
    inline SpeedLevel computeSpeedLevel(int score)
    {
        static constexpr int   POINTS_PER_LEVEL = 1000;
        static constexpr int   MAX_LEVEL        = 15;
        static constexpr float START_INTERVAL   = 0.80f;
        static constexpr float END_INTERVAL     = 0.10f;

        const int level = std::min(score / POINTS_PER_LEVEL + 1, MAX_LEVEL);
        const float t   = float(level - 1) / float(MAX_LEVEL - 1);
        return { level, START_INTERVAL * std::pow(END_INTERVAL / START_INTERVAL, t) };
    }
}
