#pragma once

#include <vector>
#include "GlmConfig.h"

// Grid-space state for a procedurally placed dungeon enemy (DungeonFloorScene).
// Patrols along a stored path using ping-pong traversal.
// EnemyMovementSystem drives smooth interpolation via fromPosition / toPosition.
struct FloorEnemyComponent
{
    int   gridX  = 0;
    int   gridZ  = 0;
    float floorY = 0.0f; // world-space Y of the floor this enemy lives on

    std::vector<glm::ivec2> patrolPath;
    int  patrolIndex   = 0;
    bool patrolForward = true;

    int  maxHealth     = 3;
    int  currentHealth = 3;
    bool alive         = true;

    bool      inTransition       = false;
    float     transitionProgress = 0.0f;
    glm::vec3 fromPosition       = {0.0f, 0.0f, 0.0f};
    glm::vec3 toPosition         = {0.0f, 0.0f, 0.0f};

    float waitTimer = 0.0f;

    static constexpr float TRANSITION_DURATION = 0.4f;
    static constexpr float PATROL_WAIT_TIME    = 1.2f;
};
