#pragma once

#include <algorithm>
#include <cstdint>

// Grid-space state for a dungeon enemy.
// Used by both the legacy dungeon scenes and the active-floor enemy movement.
struct EnemyComponent
{
    int  gridX    = 0;
    int  gridZ    = 0;
    int  floorIndex = 0;
    int  hp       = 2;
    int  maxHp    = 2;

    float    moveSpeed    = 2.5f;
    float    moveCooldown = 0.0f;
    uint32_t rngState     = 1u;
    bool     alive        = true;

    // Patrol waypoints
    int patrolX0  = 0, patrolZ0 = 0;  // endpoint A
    int patrolX1  = 0, patrolZ1 = 0;  // endpoint B
    int patrolDir = 1;                  // +1 = toward B, -1 = toward A

    static constexpr float MOVE_COOLDOWN_TIME = 1.2f;
};
