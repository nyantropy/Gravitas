#pragma once

#include <algorithm>

// Grid-space state for a dungeon enemy.
// EnemySystem patrols the enemy between (patrolX0,patrolZ0) and
// (patrolX1,patrolZ1), flipping direction each time it reaches an endpoint.
struct EnemyComponent
{
    int  gridX    = 0;
    int  gridZ    = 0;
    int  hp       = 2;
    int  maxHp    = 2;

    // Patrol waypoints
    int patrolX0  = 0, patrolZ0 = 0;  // endpoint A
    int patrolX1  = 0, patrolZ1 = 0;  // endpoint B
    int patrolDir = 1;                  // +1 = toward B, -1 = toward A

    float moveCooldown = 0.8f; // stagger first move so not all move at once
    static constexpr float MOVE_COOLDOWN_TIME = 1.2f;

    bool alive = true;
};
