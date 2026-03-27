#pragma once

// Grid-space player state for the dungeon crawler.
// Position is in grid cells. Facing: 0=North(-Z), 1=East(+X), 2=South(+Z), 3=West(-X).
struct PlayerComponent
{
    int gridX  = 3;
    int gridZ  = 3;
    int facing = 0; // 0=North, 1=East, 2=South, 3=West

    // Input cooldown — prevents holding a key from moving multiple cells per frame.
    float moveCooldown = 0.0f;
    float turnCooldown = 0.0f;

    static constexpr float MOVE_COOLDOWN_TIME = 0.18f;
    static constexpr float TURN_COOLDOWN_TIME = 0.15f;
};
