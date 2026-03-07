#pragma once

#include <cmath>
#include <algorithm>

// Maps the current score to a speed level and a corresponding fall interval.
// Owned entirely by the helpers layer — no ECS dependencies.
struct SpeedLevel
{
    int   level;        // 1-based speed level (1 = slowest, MAX_LEVEL = fastest)
    float fallInterval; // seconds between automatic gravity ticks
};

// Computes the speed level and fall interval from the current score.
//
// Progression:
//   level  = clamp(score / POINTS_PER_LEVEL + 1, 1, MAX_LEVEL)
//   interval = START_INTERVAL * pow(END_INTERVAL / START_INTERVAL, (level-1)/(MAX_LEVEL-1))
//
// Level 1  → 0.80 s/row  (gentle starting pace)
// Level 8  → ~0.28 s/row (mid-game)
// Level 15 → 0.10 s/row  (fast end-game)
inline SpeedLevel computeSpeedLevel(int score)
{
    static constexpr int   POINTS_PER_LEVEL = 1000;
    static constexpr int   MAX_LEVEL        = 15;
    static constexpr float START_INTERVAL   = 0.80f;
    static constexpr float END_INTERVAL     = 0.10f;

    const int level = std::min(score / POINTS_PER_LEVEL + 1, MAX_LEVEL);

    const float t        = float(level - 1) / float(MAX_LEVEL - 1);
    const float interval = START_INTERVAL * std::pow(END_INTERVAL / START_INTERVAL, t);

    return { level, interval };
}
