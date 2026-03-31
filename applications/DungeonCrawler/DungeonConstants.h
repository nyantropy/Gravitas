#pragma once

#include <cmath>

// Y-axis layout constants for the multi-floor dungeon.
// Floor N is offset downward by N * FLOOR_Y_OFFSET world units.
struct DungeonConstants
{
    static constexpr float FLOOR_Y_OFFSET   = 4.0f;
    static constexpr float RAMP_ANGLE_DEG   = 35.0f;

    static float floorWorldY(int floorNumber)
    {
        return -static_cast<float>(floorNumber) * FLOOR_Y_OFFSET;
    }
};
